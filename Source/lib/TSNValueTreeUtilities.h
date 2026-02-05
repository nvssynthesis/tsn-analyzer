//
// Created by Nicholas Solem on 1/30/26.
//

#pragma once
#include <juce_data_structures/juce_data_structures.h>

#include "Statistics.h"
#include "FeatureOperations.h"
#include "StringAxiom.h"
#include "TimbreAnalysis/TimbreAnalysisResult.h"
#include "AnalysisUsing.h"

namespace nvs::analysis {

[[nodiscard]]
bool validateAnalysisVT(const ValueTree &analysisSuperVT);

[[nodiscard]]
ValueTree makeSuperTree(const ValueTree &timbreSpaceTree,
    const String &sampleFilePath,
    double sampleRate,
    const String &waveformHash,
    const String &settingsHash,
    const ValueTree &settingsTree);

void addEventwiseStatistics(ValueTree& tree, const EventwiseStatisticsF& stats);

[[nodiscard]]
EventwiseStatisticsF toEventwiseStatistics(ValueTree const &vt);

[[nodiscard]]
ValueTree timbreSpaceReprToVT(std::vector<FeatureContainer<EventwiseStatisticsF>> const &fullTimbreSpace,
                                           vecReal const &normalizedOnsets);

[[nodiscard]]
std::vector<FeatureContainer<EventwiseStatisticsF>> valueTreeToTimbreSpace(ValueTree const &vt);

[[nodiscard]]
vecReal valueTreeToNormalizedOnsets(ValueTree const &vt);

[[nodiscard]]
vecReal extractFeaturesFromTree(const ValueTree &frameTree,
                        const std::vector<Feature_e> &featuresToUse,
                        Statistic statisticToUse);

// Overload for single feature - wraps it in a std::array for iteration
[[nodiscard]]
vecReal extractFeaturesFromTree(const ValueTree &frameTree,
                        Feature_e featureToUse,
                        Statistic statisticToUse);


}