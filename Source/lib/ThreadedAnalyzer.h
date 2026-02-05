/*
  ==============================================================================

    ThreadedAnalyzer.h
    Created: 1 Nov 2023 3:36:32pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "Analyzer.h"
#include "OnsetAnalysis/OnsetAnalysisResult.h"
#include "TimbreAnalysis/TimbreAnalysisResult.h"
#include <juce_core/juce_core.h>

namespace nvs::analysis {

using Thread = juce::Thread;
using ChangeBroadcaster = juce::ChangeBroadcaster;
using String = juce::String;


class ThreadedAnalyzer final :	public Thread
,							    public ChangeBroadcaster
{
public:
    ThreadedAnalyzer();
    ~ThreadedAnalyzer() override;
    //===============================================================================
    void updateStoredAudio(std::span<float const> wave, const juce::String &audioFileAbsPath);
    void updateSettings(juce::ValueTree &settingsTree, bool attemptFix);
    //===============================================================================
    void stopAnalysis() { DBG("Stopping analysis thread..."); signalThreadShouldExit(); }
    //===============================================================================
    bool onsetsReady() const {
        return _onsetAnalysisResult != nullptr;
    }
    bool timbreAnalysisReady() const {
        return _timbreAnalysisResult.has_value();
    }
    enum class State {
        Idle,
        Analyzing,
        Complete,
        Failed
    };

    State getState() const { return _state.load(); }
    //===============================================================================
    std::shared_ptr<OnsetAnalysisResult> shareOnsetAnalysis() const;
    std::optional<TimbreAnalysisResult> stealTimbreSpaceRepresentation();
    //===============================================================================
    [[deprecated("any reason we would want to get the raw analyzer, there should just be an intermediate method")]]
    Analyzer &getAnalyzer() { return _analyzer; }

    RunLoopStatus &getStatus() noexcept { return _rls; }
    String getSettingsHash() const noexcept { return _analyzer.getSettingsHash(); }
    ValueTree getSettingsParentTree() const { return _analyzer.getSettingsParentTree(); }
    //===============================================================================
private:
    Analyzer _analyzer;
    vecReal _inputWave;
    std::shared_ptr<OnsetAnalysisResult> _onsetAnalysisResult;
    std::optional<TimbreAnalysisResult> _timbreAnalysisResult;

    String _audioFileAbsPath {};

    RunLoopStatus _rls;

    std::atomic<State> _state {State::Idle};

    void run() override;
};

}
