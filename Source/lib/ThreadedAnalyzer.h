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
    void updateStoredAudioAndSettings(std::span<float const> wave, const juce::String &audioFileAbsPath,
        juce::ValueTree &settingsTree, bool attemptFix);
    //===============================================================================
    void stopAnalysis() { DBG("Stopping analysis thread..."); signalThreadShouldExit(); }
    //===============================================================================
    bool onsetsReady() const {
        return _onsetAnalysisResult != nullptr;
    }
    bool timbreAnalysisReady() const {
        return _timbreAnalysisResult != nullptr;
    }
    bool pacmapReady() const {
        return _pacmapResult != nullptr;
    }
    enum class State {
        Idle,
        Analyzing,
        Complete,
        Failed
    };

    State getState() const { return _state.load(); }
    //===============================================================================
    void setAnalysis(vecReal normOnsets,
        std::vector<FeatureContainer<EventwiseStatisticsF>> timbreSpaceRepr,
        vecVecReal pacmapMatrix,
        String waveformHash, String absPath, double sr);
    //===============================================================================
    std::shared_ptr<OnsetAnalysisResult> shareOnsetAnalysis() const;
    std::shared_ptr<TimbreAnalysisResult> shareTimbreSpaceRepresentation() const;
    std::shared_ptr<PacmapResult> sharePacmapResult() const;
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
    std::shared_ptr<TimbreAnalysisResult> _timbreAnalysisResult;
    std::shared_ptr<PacmapResult> _pacmapResult;

    String _audioFileAbsPath {};

    RunLoopStatus _rls;

    std::atomic<State> _state {State::Idle};

    //===============================================================================
    juce::String _lastAudioHash;
    juce::int64 _lastOnsetSettingsHash;
    juce::int64 _lastTimbreSettingsHash;
    juce::int64 _lastPacmapSettingsHash;

    bool _shouldComputeOnsets { true };
    bool _shouldComputeTimbre { true };
    bool _shouldComputePacmap { true };

    void run() override;
};

}
