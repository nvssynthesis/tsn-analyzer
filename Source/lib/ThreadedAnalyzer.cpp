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

ThreadedAnalyzer::ThreadedAnalyzer()
	:	juce::Thread("Analyzer")
{}
ThreadedAnalyzer::~ThreadedAnalyzer(){
	stopThread(10000);
}

void ThreadedAnalyzer::updateStoredAudio(std::span<float const> wave, const juce::String &audioFileAbsPath) {
    jassert(!isThreadRunning());
    _state = State::Idle;
    sendChangeMessage();

	_inputWave.assign(wave.begin(), wave.end());
	_audioFileAbsPath = audioFileAbsPath;
    _onsetAnalysisResult.reset();
    _timbreAnalysisResult.reset();
}
auto ThreadedAnalyzer::shareOnsetAnalysis() const -> std::shared_ptr<OnsetAnalysisResult> {
    return _onsetAnalysisResult;
}
auto ThreadedAnalyzer::stealTimbreSpaceRepresentation() -> std::optional<TimbreAnalysisResult> {
    return std::exchange(_timbreAnalysisResult, std::nullopt);
}

void ThreadedAnalyzer::updateSettings(juce::ValueTree &settingsTree, const bool attemptFix){
    jassert(!isThreadRunning());

    jassert( settingsTree.hasType(nvs::axiom::tsn::Settings) );
    _state = State::Idle;
    sendChangeMessage();

	if (!_analyzer.updateSettings(settingsTree, attemptFix))
	{
	    DBG("updateSettings failed");
	    jassertfalse;
	}
}


void ThreadedAnalyzer::run() {
	// first, clear everything so that if any analysis is terminated early, we don't have garbage leftover
    _state = State::Idle;
    sendChangeMessage();

    _onsetAnalysisResult.reset();
    _timbreAnalysisResult.reset();
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

		// perform onset analysis
		_rls.set("Calculating Onsets...");
	    const String audioHash = util::hashAudioData(_inputWave);

	    const auto sr = _analyzer.getAnalyzedFileSampleRate();

	    const auto unnormalizedOnsets = [this, shouldExit, audioHash, sr]()-> vecReal {
	        const auto onsetOpt = _analyzer.calculateOnsetsInSeconds(_inputWave, _rls, shouldExit);
	        if (threadShouldExit() || onsetOpt.value().empty()) {
	            DBG("Threaded Analyzer: exit requested");
                _state = State::Failed;
	            sendChangeMessage();
	            return {};
	        }
	        jassert(onsetOpt.has_value());

		    _onsetAnalysisResult = std::make_shared<OnsetAnalysisResult>(onsetOpt.value(), audioHash, _audioFileAbsPath, sr);

		    const auto lengthInSeconds = getLengthInSeconds(_inputWave.size(), sr);

		    filterOnsets(_onsetAnalysisResult->onsets, lengthInSeconds);
		    forceMinimumOnsets(_onsetAnalysisResult->onsets, 4, lengthInSeconds);

		    const auto retval = _onsetAnalysisResult->onsets;
		    normalizeOnsets(_onsetAnalysisResult->onsets, lengthInSeconds);
	        jassert(_state == State::Analyzing);
		    sendChangeMessage();    // signal that onsets are ready
	        return retval;
	    }();
	    if (unnormalizedOnsets.empty() || threadShouldExit()) {
	        DBG("Threaded Analyzer: exit requested");
	        _state = State::Failed;
	        sendChangeMessage();
	        return;
	    }

        // perform onsetwise BFCC analysis
		_rls.set("Calculating Onsetwise TimbreSpace...");
	    {
	        const auto timbreMeasurementsOpt = _analyzer.calculateOnsetwiseTimbreSpace(_inputWave, unnormalizedOnsets, _rls, shouldExit);
		    if (!timbreMeasurementsOpt.has_value() || threadShouldExit()) {
		        DBG("Threaded Analyzer: exit requested");
		        _state = State::Failed;
		        sendChangeMessage();
		        return;
		    }

		    jassert (sr == _analyzer.getAnalyzedFileSampleRate());  // sr should not have possibly changed... sanity check
		    _timbreAnalysisResult.emplace(timbreMeasurementsOpt.value(), audioHash, _audioFileAbsPath, sr);
		    _state = State::Complete;
		    sendChangeMessage();    // signal that timbre analyses are ready
	    }
	} catch (const essentia::EssentiaException& e) {
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
