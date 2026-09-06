/*
  ==============================================================================

    ThreadedAnalyzer.cpp
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "ThreadedAnalyzer.h"
#include "OnsetAnalysis/OnsetProcessing.h"
#include "StringAxiom.h"
#include <juce_utils.h>

namespace nvs::analysis {

namespace {
juce::int64 hashBranch(const juce::ValueTree& parent, const String &branch) {
    if (const auto child = parent.getChildWithName(branch); child.isValid()) {
        return child.toXmlString().hashCode64();
    }
    jassertfalse;
    return {};
}

juce::int64 computeOverallTimbreSettingsHash(const ValueTree &settingsTree) {
    const auto hash = [settingsTree](const String &s) {
        return hashBranch(settingsTree, s);
    };
    using namespace axiom::tsn;

    const auto analysisHash = hash(Analysis);
    const auto bfccHash = hash(BFCC);
    const auto pitchHash = hash(Pitch);
    const auto loudnessHash = hash(Loudness);
    const auto splitHash = hash(Split);

    const auto retval = analysisHash ^ bfccHash ^ pitchHash ^ loudnessHash ^ splitHash;
    return retval;
}
}


ThreadedAnalyzer::ThreadedAnalyzer()
	:	juce::Thread("Analyzer")
{}
ThreadedAnalyzer::~ThreadedAnalyzer(){
	stopThread(10000);
}

bool ThreadedAnalyzer::updateStoredAudioAndSettings(
    const SampleManager &sampleManager,
    juce::ValueTree &settingsTree, const bool shouldOverwriteTreeIfUpdated)
{
    jassert(!isThreadRunning());
    jassert( settingsTree.hasType(nvs::axiom::tsn::Settings) );

    _state = State::Idle;
    sendChangeMessage();

    // inform self if we truly need to do any new analysis – did EITHER the waveform hash OR the settings hash change?
    _sampleManager = sampleManager;

    _onsetAnalysisResult.reset();
    _shouldComputeOnsets = true;
    _timbreAnalysisResult.reset();
    _shouldComputeTimbre = true;
    _pacmapResult.reset();
    _shouldComputePacmap = true;

    _analysisFile = File{};

    bool treeUpdated = false;
    ValueTree updatedSettingsTree = _analyzer.updateSettings(settingsTree);
    if (!updatedSettingsTree.isValid()) {
        updatedSettingsTree = settingsTree;
    }
    if (shouldOverwriteTreeIfUpdated) {
        if (treeUpdated = !settingsTree.isEquivalentTo(updatedSettingsTree)) {
            settingsTree = updatedSettingsTree;
        }
    }

    // recompute the per‑branch hashes, to see if we can skip parts of analysis in the next run
    if (const auto parent = _analyzer.getSettingsParentTree();
        parent.isValid())
    {
        if (const auto onsetSettingsHash = hashBranch(updatedSettingsTree, axiom::tsn::Onset);
            onsetSettingsHash == _lastOnsetSettingsHash && _onsetAnalysisResult != nullptr)
        {
            _shouldComputeOnsets = false;
        } else {
            _shouldComputeOnsets = true;
            _lastOnsetSettingsHash = onsetSettingsHash;
            _onsetAnalysisResult.reset();

            _shouldComputeTimbre = true;
            _lastTimbreSettingsHash = computeOverallTimbreSettingsHash(updatedSettingsTree);
            _timbreAnalysisResult.reset();

            _shouldComputePacmap = true;
            _lastPacmapSettingsHash = hashBranch(updatedSettingsTree, axiom::tsn::PaCMAP);
            _pacmapResult.reset();
        }

        if (const auto timbreSettingsHash = computeOverallTimbreSettingsHash(updatedSettingsTree);
            timbreSettingsHash == _lastTimbreSettingsHash && _timbreAnalysisResult != nullptr)
        {
            _shouldComputeTimbre = false;
        } else {
            _shouldComputeTimbre = true;
            _lastTimbreSettingsHash = timbreSettingsHash;
            _timbreAnalysisResult.reset();

            _shouldComputePacmap = true;
            _lastPacmapSettingsHash = hashBranch(updatedSettingsTree, axiom::tsn::PaCMAP);
            _pacmapResult.reset();
        }

        if (const auto pacmapSettingsHash = hashBranch(updatedSettingsTree, axiom::tsn::PaCMAP);
            pacmapSettingsHash == _lastPacmapSettingsHash && _pacmapResult != nullptr)
        {
            _shouldComputePacmap = false;
        } else {
            _shouldComputePacmap = true;
            _lastPacmapSettingsHash = pacmapSettingsHash;
            _pacmapResult.reset();
        }
    }
    return treeUpdated;
}

void ThreadedAnalyzer::setAnalysis(vecReal normOnsets,
    std::vector<FeatureContainer<EventwiseStatisticsF>> timbreSpaceRepr,
    vecVecReal pacmapMatrix,
    String waveformHash,
    String audioAbsPath,
    const File &analysisFile,
    double sr)
{
    _onsetAnalysisResult = std::make_shared<OnsetAnalysisResult>(normOnsets, waveformHash, audioAbsPath, sr);
    // specificLoudness isn't persisted yet (see TimbreAnalysisResult.h), so a directly-set analysis
    // (e.g. loaded from a previously-saved ValueTree) has none available -- zero-fill, parallel-indexed.
    std::vector<std::array<float, NumSpecificLoudnessBands>> specificLoudness(timbreSpaceRepr.size());
    _timbreAnalysisResult = std::make_shared<TimbreAnalysisResult>(timbreSpaceRepr, specificLoudness, waveformHash, audioAbsPath, sr);
    _pacmapResult = std::make_shared<PacmapResult>(pacmapMatrix, waveformHash, audioAbsPath, sr);
    _shouldComputeOnsets = false;
    _shouldComputeTimbre = false;
    _shouldComputePacmap = false;
    _analysisFile = analysisFile;
}

auto ThreadedAnalyzer::shareOnsetAnalysis() const -> std::shared_ptr<OnsetAnalysisResult> {
    return _onsetAnalysisResult;
}
auto ThreadedAnalyzer::shareTimbreSpaceRepresentation() const -> std::shared_ptr<TimbreAnalysisResult> {
    return _timbreAnalysisResult;
}
auto ThreadedAnalyzer::sharePacmapResult() const -> std::shared_ptr<PacmapResult> {
    return _pacmapResult;
}

void ThreadedAnalyzer::run() {
    auto report = [this](const String &s) {
        _rls.set(s);
        Logger::writeToLog(s);
    };

    if (!_shouldComputePacmap) {    // nothing to compute
        jassert (!_shouldComputeTimbre);
        jassert (!_shouldComputeOnsets);
        _state = State::Complete;
        sendChangeMessage();
        return;
    }
    _state = State::Idle;
    _analysisFile = File();
    sendChangeMessage();

	if (!_sampleManager.hasValidAudio()){
		return;
	}
	_rls.set(0.0);

    _state = State::Analyzing;
    sendChangeMessage();    // to signal STARTING analysis
	try {
		// let any sub-step know if we’ve been asked to exit:
		auto shouldExit = [this]() {
			const bool retval = threadShouldExit();
			if (retval){
				DBG("ThreadedAnalyzer: exit requested");
			}
			return retval;
		};
        const auto waveform = [this]() {
            const auto waveSpan = _sampleManager.getChannelSpan(0);
            return vecReal(waveSpan.begin(), waveSpan.end());
        }();
	    const auto unnormalizedOnsets = [this, &waveform, shouldExit, &report]()-> vecReal {
	        // perform onset analysis
	        report("Calculating Onsets...");
	        const auto lengthInSeconds = getLengthInSeconds(_sampleManager.getLength(), _sampleManager.getSampleRate());

	        if (!_shouldComputeOnsets) {
	            // return existing onsets but unnormalized
	            jassert(_onsetAnalysisResult != nullptr);
	            sendChangeMessage();    // signal that onsets are ready
	            auto onsetsCpy = _onsetAnalysisResult->onsets;
	            denormalizeOnsets(onsetsCpy, lengthInSeconds);

	            return onsetsCpy;
	        }

	        const auto onsetOpt = _analyzer.calculateOnsetsInSeconds(
	            waveform, _sampleManager.getSampleRate(),
	            _rls, shouldExit);
	        if (threadShouldExit() || onsetOpt.value().empty()) {
	            DBG("Threaded Analyzer: exit requested");
                _state = State::Failed;
	            sendChangeMessage();
	            return {};
	        }
	        jassert(onsetOpt.has_value());

		    _onsetAnalysisResult = std::make_shared<OnsetAnalysisResult>(onsetOpt.value(),
		        _sampleManager.getWaveformHash(), _sampleManager.getFullPath(), _sampleManager.getSampleRate());

	        const auto unnormCpy = [this, &waveform, lengthInSeconds, &report](){
	            report("Processing onsets..");

	            namespace ax = axiom::tsn;
                if (const auto& onsetSettings =_analyzer.getSettings().get<modern::OnsetSettings>();
                    onsetSettings.getBoolValue(ax::doRefinements))
                {
	                improveOnsetsInSeconds(_onsetAnalysisResult->onsets, waveform, _sampleManager.getSampleRate());

	                filterOnsetsOutsideBounds(_onsetAnalysisResult->onsets, lengthInSeconds);
	                filterRedundantOnsets(_onsetAnalysisResult->onsets);


	                subdivideOnsetsEnergy(_onsetAnalysisResult->onsets, waveform, _sampleManager.getSampleRate(), onsetSettings.getIntValue(ax::refinementNumEventSubdivisions));

	                const auto silenceMarkers = detectSilences(
	                    waveform, _sampleManager.getSampleRate(),
	                    onsetSettings.getFloatValue(ax::refinementSilenceThresholdDb),
	                    onsetSettings.getFloatValue(ax::refinementMinSilenceDurationMs),
	                    onsetSettings.getFloatValue(ax::refinementMinEventWithinSilenceDurationMs));

	                combineOnsetsAndSilenceTimings(_onsetAnalysisResult->onsets, silenceMarkers,
                        0.2, 0.2,
                        0.5, 0.2);

	                filterOnsetsOutsideBounds(_onsetAnalysisResult->onsets, lengthInSeconds);  // after inserting new onsets, its possible again that some are too bunched up
	            }

	            forceMinimumOnsets(_onsetAnalysisResult->onsets, 4, lengthInSeconds);

	            const auto retval = _onsetAnalysisResult->onsets;   // get copy of (still UNNORMALIZED) onsets
	            normalizeOnsets(_onsetAnalysisResult->onsets, lengthInSeconds); // normalize member
    		    sendChangeMessage();    // signal that onsets are ready
	            return retval;
	        }();
	        jassert(_state == State::Analyzing);
	        return unnormCpy;
	    }();
	    if (unnormalizedOnsets.empty() || threadShouldExit()) {
	        DBG("Threaded Analyzer: exit requested");
	        _state = State::Failed;
	        sendChangeMessage();
	        return;
	    }
	    [this, &waveform, &report, &shouldExit](const std::vector<float> &unnormOnsets){
	        // perform onsetwise timbral analysis
		    if (!_shouldComputeTimbre) {
		        jassert(_timbreAnalysisResult != nullptr);
		        sendChangeMessage();    // signal that timbres are ready
		        return;
		    }
		    report("Calculating Onsetwise TimbreSpace...");

		    std::vector<std::array<float, NumSpecificLoudnessBands>> specificLoudness;
		    const auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(
		        waveform, _sampleManager.getSampleRate(), unnormOnsets, _rls, shouldExit, specificLoudness);
		    if (!timbreMeasurementsOpt.has_value() || threadShouldExit()) {
		        DBG("Threaded Analyzer: exit requested");
		        _state = State::Failed;
		        sendChangeMessage();
		        return;
		    }

	        _timbreAnalysisResult = std::make_shared<TimbreAnalysisResult>(
	            timbreMeasurementsOpt.value(),
	            specificLoudness,
	            _sampleManager.getWaveformHash(),
	            _sampleManager.getFullPath(),
	            _sampleManager.getSampleRate());
	    }(unnormalizedOnsets);

	    if (threadShouldExit()) {
	        DBG("Threaded Analyzer: exit requested");
	        _state = State::Failed;
	        sendChangeMessage();
	        return;
	    }

	    jassert(_shouldComputePacmap);  // otherwise we should have returned at the very start
        report("Calculating PaCMAP Timbre Space...");
        _rls.set(0.5);
        if (const auto pacmapMatrix = _analyzer.calculatePaCMAP(_timbreAnalysisResult->timbreMeasurements);
            pacmapMatrix.has_value())
        {
            _pacmapResult = std::make_shared<PacmapResult>(*pacmapMatrix,
                _sampleManager.getWaveformHash(), _sampleManager.getFullPath(), _sampleManager.getSampleRate());
        } else {
            report("Not enough points for PaCMAP, skipping...");
            jassert(_pacmapResult == nullptr);
        }

		_state = State::Complete;
		sendChangeMessage();    // signal that timbre analyses are ready

	} catch (const EssentiaException& e) {
		DBG("Essentia exception: " << e.what());
	    _state = State::Failed;
		sendChangeMessage(); // Let GUI know something changed
		return;
	}
	catch (const std::exception& e) {
		DBG("Standard exception in analysis thread: " << e.what());
	    _state = State::Failed;
		sendChangeMessage(); // Let GUI know something changed
		return;
	} catch (...) {
		DBG("Unknown exception in analysis thread");
	    _state = State::Failed;
		sendChangeMessage(); // Let GUI know something changed
		return;
	}
}

}
