//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include "AnalysisUsing.h"

namespace nvs::analysis {

struct TimbreAnalysisResult {
    TimbreAnalysisResult(std::vector<FeatureContainer<EventwiseStatisticsF>> timbreMeasurements_,
        juce::String hash_,
        juce::String path_)
    :   timbreMeasurements(std::move(timbreMeasurements_))
    , waveformHash(std::move(hash_))
    , audioFileAbsPath(std::move(path_))
    {}

    std::vector<FeatureContainer<EventwiseStatisticsF>> timbreMeasurements;

    juce::String waveformHash {};
    juce::String audioFileAbsPath {};
};

} // namespace nvs::analysis
