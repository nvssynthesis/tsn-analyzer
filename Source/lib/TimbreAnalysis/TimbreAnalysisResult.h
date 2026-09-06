//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include "AnalysisUsing.h"
#include "LoudnessAnalysis/Iso532Loudness.h"

namespace nvs::analysis {

struct TimbreAnalysisResult {
    TimbreAnalysisResult(std::vector<FeatureContainer<EventwiseStatisticsF>> timbreMeasurements_,
        std::vector<std::array<float, NumSpecificLoudnessBands>> specificLoudness_,
        juce::String hash_,
        juce::String path_,
        const double sampleRate_)
    :   timbreMeasurements(std::move(timbreMeasurements_))
    , specificLoudness(std::move(specificLoudness_))
    , waveformHash(std::move(hash_))
    , audioFileAbsPath(std::move(path_))
    , sampleRate(sampleRate_)
    {}

    std::vector<FeatureContainer<EventwiseStatisticsF>> timbreMeasurements;

    // ISO 532-1 specific-loudness mean vector per onset, parallel-indexed to timbreMeasurements
    // (not persisted to ValueTree yet, not part of Feature_e -- see Iso532Loudness.h).
    std::vector<std::array<float, NumSpecificLoudnessBands>> specificLoudness;

    juce::String waveformHash {};
    juce::String audioFileAbsPath {};
    double sampleRate {};
};


struct PacmapResult {
    PacmapResult(vecVecReal pacmapMatrix,
        juce::String hash_,
        juce::String path_,
        const double sampleRate_)
    :   pacmapMatrix_(std::move(pacmapMatrix))
    , waveformHash(std::move(hash_))
    , audioFileAbsPath(std::move(path_))
    , sampleRate(sampleRate_)
    {}

    vecVecReal pacmapMatrix_;

    juce::String waveformHash {};
    juce::String audioFileAbsPath {};
    double sampleRate {};
};

} // namespace nvs::analysis
