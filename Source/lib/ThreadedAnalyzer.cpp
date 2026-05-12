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

void ThreadedAnalyzer::updateStoredAudioAndSettings(std::span<float const> wave, const juce::String &audioFileAbsPath,
    juce::ValueTree &settingsTree, const bool attemptFix)
{
    jassert(!isThreadRunning());
    jassert( settingsTree.hasType(nvs::axiom::tsn::Settings) );

    _state = State::Idle;
    sendChangeMessage();

	_audioFileAbsPath = audioFileAbsPath;   // always update; it's possible that the audio file moved even tho it's the same content
    if (const auto audioHash = util::hashAudioData(_inputWave);
        audioHash == _lastAudioHash)
    {
        _onsetAnalysisResult.reset();
        _shouldComputeOnsets = true;
        _timbreAnalysisResult.reset();
        _shouldComputeTimbre = true;
        _pacmapResult.reset();
        _shouldComputePacmap = true;
    } else {
        _inputWave.assign(wave.begin(), wave.end());
    }

    if (!_analyzer.updateSettings(settingsTree, attemptFix))    // 'blindly' update all settings of analyzer
    {
        DBG("updateSettings failed");
        jassertfalse;
    }

    // recompute the per‑branch hashes, to see if we can skip parts of analysis in the next run
    if (const auto parent = _analyzer.getSettingsParentTree();
        parent.isValid())
    {

        if (const auto onsetSettingsHash = hashBranch(settingsTree, axiom::tsn::Onset);
            onsetSettingsHash == _lastOnsetSettingsHash && _onsetAnalysisResult != nullptr)
        {
            _shouldComputeOnsets = false;
        } else {
            _shouldComputeOnsets = true;
            _lastOnsetSettingsHash = onsetSettingsHash;
            _onsetAnalysisResult.reset();

            _shouldComputeTimbre = true;
            _lastTimbreSettingsHash = computeOverallTimbreSettingsHash(settingsTree);
            _timbreAnalysisResult.reset();

            _shouldComputePacmap = true;
            _lastPacmapSettingsHash = hashBranch(settingsTree, axiom::tsn::PaCMAP);
            _pacmapResult.reset();
        }

        if (const auto timbreSettingsHash = computeOverallTimbreSettingsHash(settingsTree);
            timbreSettingsHash == _lastTimbreSettingsHash && _timbreAnalysisResult != nullptr)
        {
            _shouldComputeTimbre = false;
        } else {
            _shouldComputeTimbre = true;
            _lastTimbreSettingsHash = timbreSettingsHash;
            _timbreAnalysisResult.reset();

            _shouldComputePacmap = true;
            _lastPacmapSettingsHash = hashBranch(settingsTree, axiom::tsn::PaCMAP);
            _pacmapResult.reset();
        }

        if (const auto pacmapSettingsHash = hashBranch(settingsTree, axiom::tsn::PaCMAP);
            pacmapSettingsHash == _lastPacmapSettingsHash && _pacmapResult != nullptr)
        {
            _shouldComputePacmap = false;
        } else {
            _shouldComputePacmap = true;
            _lastPacmapSettingsHash = pacmapSettingsHash;
            _pacmapResult.reset();
        }
    }
}

void ThreadedAnalyzer::setAnalysis(vecReal normOnsets,
    std::vector<FeatureContainer<EventwiseStatisticsF>> timbreSpaceRepr,
    vecVecReal pacmapMatrix,
    String waveformHash, String absPath, double sr) {
    _onsetAnalysisResult = std::make_shared<OnsetAnalysisResult>(normOnsets, waveformHash, absPath, sr);
    _timbreAnalysisResult = std::make_shared<TimbreAnalysisResult>(timbreSpaceRepr, waveformHash, absPath, sr);
    _pacmapResult = std::make_shared<PacmapResult>(pacmapMatrix, waveformHash, absPath, sr);
    _shouldComputeOnsets = false;
    _shouldComputeTimbre = false;
    _shouldComputePacmap = false;
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
    sendChangeMessage();

	if (!(_inputWave.data() && !_inputWave.empty())){
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
			return retval;;
		};

	    const String audioHash = util::hashAudioData(_inputWave);

	    const auto sr = _analyzer.getAnalyzedFileSampleRate();

	    const auto unnormalizedOnsets = [this, shouldExit, audioHash, sr, &report]()-> vecReal {
	        // perform onset analysis
	        report("Calculating Onsets...");
	        const auto lengthInSeconds = getLengthInSeconds(_inputWave.size(), sr);

	        if (!_shouldComputeOnsets) {
	            // return existing onsets but unnormalized
	            jassert(_onsetAnalysisResult != nullptr);
	            sendChangeMessage();    // signal that onsets are ready
	            auto onsetsCpy = _onsetAnalysisResult->onsets;
	            denormalizeOnsets(onsetsCpy, lengthInSeconds);

	            return onsetsCpy;
	        }

	        const auto onsetOpt = _analyzer.calculateOnsetsInSeconds(_inputWave, _rls, shouldExit);
	        if (threadShouldExit() || onsetOpt.value().empty()) {
	            DBG("Threaded Analyzer: exit requested");
                _state = State::Failed;
	            sendChangeMessage();
	            return {};
	        }
	        jassert(onsetOpt.has_value());

		    _onsetAnalysisResult = std::make_shared<OnsetAnalysisResult>(onsetOpt.value(), audioHash, _audioFileAbsPath, sr);

	        const auto unnormCpy = [this, sr, lengthInSeconds, &report](){
	            report("Processing onsets..");

	            improveOnsetsInSeconds(_onsetAnalysisResult->onsets, _inputWave, sr);

	            filterOnsetsOutsideBounds(_onsetAnalysisResult->onsets, lengthInSeconds);
	            filterRedundantOnsets(_onsetAnalysisResult->onsets);

	            const auto &[numEventSubdivisions,
                    silenceThresholdDb,
                    minSilenceDurationMs,
                    minEventWithinSilenceDurationMs]
                    = _analyzer.getSettings().onset._refinement;
	            subdivideOnsetsEnergy(_onsetAnalysisResult->onsets, _inputWave, sr, numEventSubdivisions);

	            const auto silenceMarkers = detectSilences(_inputWave, sr,
                    silenceThresholdDb, minSilenceDurationMs,
                    minEventWithinSilenceDurationMs);

	            combineOnsetsAndSilenceTimings(_onsetAnalysisResult->onsets, silenceMarkers,
                    0.2, 0.2,
                    0.5, 0.2);

	            filterOnsetsOutsideBounds(_onsetAnalysisResult->onsets, lengthInSeconds);  // after inserting new onsets, its possible again that some are too bunched up

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
	    [this, &report, &shouldExit, sr, audioHash](const std::vector<float> &unnormOnsets){
	        // perform onsetwise timbral analysis
		    if (!_shouldComputeTimbre) {
		        jassert(_timbreAnalysisResult != nullptr);
		        sendChangeMessage();    // signal that timbres are ready
		        return;
		    }
		    report("Calculating Onsetwise TimbreSpace...");
		    const auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(_inputWave, unnormOnsets, _rls, shouldExit);
		    if (!timbreMeasurementsOpt.has_value() || threadShouldExit()) {
		        DBG("Threaded Analyzer: exit requested");
		        _state = State::Failed;
		        sendChangeMessage();
		        return;
		    }

		    jassert (sr == _analyzer.getAnalyzedFileSampleRate());  // sr should not have possibly changed... sanity check
	        _timbreAnalysisResult = std::make_shared<TimbreAnalysisResult>(timbreMeasurementsOpt.value(), audioHash, _audioFileAbsPath, sr);
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
            _pacmapResult = std::make_shared<PacmapResult>(*pacmapMatrix, audioHash, _audioFileAbsPath, sr);
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
