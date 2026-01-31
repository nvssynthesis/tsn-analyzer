//
// Created by Nicholas Solem on 1/30/26.
//

#pragma once
#include <juce_data_structures/juce_data_structures.h>

#include "Statistics.h"
#include "FeatureOperations.h"
#include "JuceHeader.h"
#include "TimbreAnalysis/TimbreAnalysisResult.h"
#include "StringAxiom.h"

namespace nvs::analysis {

using ValueTree = juce::ValueTree;


inline void addEventwiseStatistics(juce::ValueTree& tree, const EventwiseStatisticsF& stats) {
    tree.setProperty(axiom::tsn::mean, stats.mean, nullptr);
    tree.setProperty(axiom::tsn::median, stats.median, nullptr);
    tree.setProperty(axiom::tsn::variance, stats.variance, nullptr);
    tree.setProperty(axiom::tsn::skewness, stats.skewness, nullptr);
    tree.setProperty(axiom::tsn::kurtosis, stats.kurtosis, nullptr);
}

inline EventwiseStatisticsF toEventwiseStatistics(juce::ValueTree const &vt){
    return {
        .mean = vt.getProperty(axiom::tsn::mean),
        .median = vt.getProperty(axiom::tsn::median),
        .variance = vt.getProperty(axiom::tsn::variance),
        .skewness = vt.getProperty(axiom::tsn::skewness),
        .kurtosis = vt.getProperty(axiom::tsn::kurtosis)
    };
}

inline juce::ValueTree timbreSpaceReprToVT(std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>> const &fullTimbreSpace,
                                           std::vector<float> const &normalizedOnsets,
                                           const juce::String& waveformHash,
                                           const juce::String& audioAbsPath){
    ValueTree vt(axiom::tsn::TimbreAnalysis);
    {
        ValueTree md(axiom::tsn::Metadata);
        md.setProperty(axiom::tsn::Version, ProjectInfo::versionString, nullptr);
        md.setProperty(axiom::tsn::audioHash, waveformHash, nullptr);
        md.setProperty(axiom::tsn::AudioFilePathAbsolute, audioAbsPath, nullptr);
        md.setProperty(axiom::tsn::CreationTime, {}, nullptr);
        md.setProperty(axiom::tsn::AnalysisSettings, {}, nullptr);
        vt.addChild(md, 0, nullptr);
    }
    {
        var onsetArray;
        for (auto const &o : normalizedOnsets) {
            onsetArray.append(o);
        }
        vt.setProperty(axiom::tsn::NormalizedOnsets, onsetArray, nullptr);
    }
    {
        ValueTree timbreMeasurements("TimbreMeasurements");

        for (int frameIdx = 0; frameIdx < static_cast<int>(fullTimbreSpace.size()); ++frameIdx){
            const auto &timbreFrame = fullTimbreSpace[frameIdx];

            ValueTree frameTree(axiom::tsn::Frame);

            ValueTree bfccsTree(axiom::tsn::BFCCs);
            {
                const auto &bfccs = timbreFrame.bfccs();
                for (int bfccIdx = 0; bfccIdx < static_cast<int>(bfccs.size()); ++bfccIdx){
                    ValueTree bfccTree("BFCC" + juce::String(bfccIdx));
                    addEventwiseStatistics(bfccTree, bfccs[bfccIdx]);
                    bfccsTree.addChild(bfccTree, bfccIdx, nullptr);
                }
            }
            frameTree.addChild(bfccsTree, -1, nullptr);

            // Add single-value features
            for (auto feature : util::Iterator<analysis::Feature_e, static_cast<analysis::Feature_e>(analysis::NumBFCC), analysis::Feature_e::f0>()) {
                ValueTree featureTree(analysis::toString(feature));
                addEventwiseStatistics(featureTree, timbreFrame[feature]);
                frameTree.addChild(featureTree, -1, nullptr);
            }

            timbreMeasurements.addChild(frameTree, frameIdx, nullptr);

            vt.addChild(timbreMeasurements, 1, nullptr);
        }
    }

    return vt;
}

inline std::vector<nvs::analysis::FeatureContainer<EventwiseStatisticsF>> valueTreeToTimbreSpace(juce::ValueTree const &vt)
{
    using namespace analysis;

    std::vector<FeatureContainer<EventwiseStatisticsF>> timbreSpace;

    auto timbreMeasurements = vt.getChildWithName(axiom::tsn::TimbreMeasurements);
    if (!timbreMeasurements.isValid())
        return timbreSpace;

    // Reserve space for efficiency
    timbreSpace.reserve(timbreMeasurements.getNumChildren());

    static_assert(static_cast<Feature_e>(0) == Feature_e::bfcc0); // we will be casting ints to Features for the BFCCs
    static_assert(static_cast<Feature_e>(12) == Feature_e::bfcc12);
    for (int frameIdx = 0; frameIdx < timbreMeasurements.getNumChildren(); ++frameIdx)
    {
        auto frameTree = timbreMeasurements.getChild(frameIdx);
        FeatureContainer<EventwiseStatisticsF> frame;

        // Extract BFCCs
        if (auto bfccsTree = frameTree.getChildWithName(axiom::tsn::BFCCs);
            bfccsTree.isValid())
        {
            for (int bfccIdx = 0; bfccIdx < bfccsTree.getNumChildren(); ++bfccIdx)
            {
                auto bfccTree = bfccsTree.getChild(bfccIdx);
                frame[static_cast<Feature_e>(bfccIdx)] = (toEventwiseStatistics(bfccTree));
            }
        }

        // Extract single-value features
        for (auto const feature :  nvs::util::Iterator<Feature_e, static_cast<Feature_e>(NumBFCC), Feature_e::f0>()) {
            if (auto featureTree = frameTree.getChildWithName(toString(feature));
                featureTree.isValid())
            {
                frame[feature] = toEventwiseStatistics(featureTree);
            }
        }

        timbreSpace.push_back(std::move(frame));
    }

    return timbreSpace;
}

inline std::vector<float> valueTreeToNormalizedOnsets(juce::ValueTree const &vt)
{
    std::vector<float> normalizedOnsets;

    const auto onsetArray = vt.getProperty(axiom::tsn::NormalizedOnsets);
    if (!onsetArray.isArray())
        return normalizedOnsets;

    auto* array = onsetArray.getArray();
    if (!array)
        return normalizedOnsets;

    normalizedOnsets.reserve(array->size());

    for (auto && e : *array)
    {
        normalizedOnsets.push_back(static_cast<float>(e));
    }

    return normalizedOnsets;
}

}