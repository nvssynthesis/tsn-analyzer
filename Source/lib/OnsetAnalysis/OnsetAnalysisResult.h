//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include <juce_core/juce_core.h>

namespace nvs::analysis {

struct OnsetAnalysisResult {
    OnsetAnalysisResult(std::vector<float> onsets_, juce::String hash_, juce::String path_, const double sampleRate_)
    :   onsets(std::move(onsets_)), waveformHash(std::move(hash_)), audioFileAbsPath(std::move(path_)), sampleRate(sampleRate_) {}

    std::vector<float> onsets;

    juce::String waveformHash {};
    juce::String audioFileAbsPath {};
    double sampleRate {0.0};
};

} // namespace nvs::analysis