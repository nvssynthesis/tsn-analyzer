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

bool validateAnalysisVT(const juce::ValueTree &analysisVT);

ValueTree makeSuperTree(const ValueTree &timbreSpaceTree,
    const String &sampleFilePath,
    double sampleRate,
    const String &waveformHash,
    const String &settingsHash);

void addEventwiseStatistics(juce::ValueTree& tree, const EventwiseStatisticsF& stats);

[[nodiscard]]
EventwiseStatisticsF toEventwiseStatistics(juce::ValueTree const &vt);

[[nodiscard]]
juce::ValueTree timbreSpaceReprToVT(std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>> const &fullTimbreSpace,
                                           std::vector<Real> const &normalizedOnsets,
                                           const juce::String& waveformHash,
                                           const juce::String& audioAbsPath);

[[nodiscard]]
std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>> valueTreeToTimbreSpace(juce::ValueTree const &vt);

[[nodiscard]]
std::vector<Real> valueTreeToNormalizedOnsets(juce::ValueTree const &vt);

[[nodiscard]]
std::vector<Real>
extractFeaturesFromTree(const juce::ValueTree &frameTree,
                        const std::vector<Feature_e> &featuresToUse,
                        Statistic statisticToUse);

// Overload for single feature - wraps it in a std::array for iteration
[[nodiscard]]
std::vector<Real>
extractFeaturesFromTree(const juce::ValueTree &frameTree,
                        Feature_e featureToUse,
                        Statistic statisticToUse);


}