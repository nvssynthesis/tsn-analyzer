//
// Created by Nicholas Solem on 1/31/26.
//

#pragma once

#include "TSNValueTreeUtilities.h"
#include "version.h"
#include "juce_utils.h"

namespace nvs::analysis {

bool validateAnalysisVT(const juce::ValueTree &analysisVT) {
    if (const auto analysisFileTree = analysisVT.getChildWithName(nvs::axiom::tsn::TimbreAnalysis);
    analysisFileTree.isValid())
    {
        DBG(fmt::format("tree being set: {}", nvs::util::valueTreeToXmlStringSafe(analysisFileTree).toStdString()));
        // we need to know if we even SHOULD load the analysisFile pointed to by the state
        if (const auto metadataTree = analysisFileTree.getChildWithName(nvs::axiom::tsn::Metadata);
            metadataTree.isValid())
        {
            return true;
        }
    }
    Logger::writeToLog("analysis file tree invalid");
    return false;
}

void addEventwiseStatistics(juce::ValueTree& tree, const EventwiseStatisticsF& stats) {
    tree.setProperty(axiom::tsn::mean, stats.mean, nullptr);
    tree.setProperty(axiom::tsn::median, stats.median, nullptr);
    tree.setProperty(axiom::tsn::variance, stats.variance, nullptr);
    tree.setProperty(axiom::tsn::skewness, stats.skewness, nullptr);
    tree.setProperty(axiom::tsn::kurtosis, stats.kurtosis, nullptr);
}

EventwiseStatisticsF toEventwiseStatistics(juce::ValueTree const &vt){
    return {
        .mean = vt.getProperty(axiom::tsn::mean),
        .median = vt.getProperty(axiom::tsn::median),
        .variance = vt.getProperty(axiom::tsn::variance),
        .skewness = vt.getProperty(axiom::tsn::skewness),
        .kurtosis = vt.getProperty(axiom::tsn::kurtosis)
    };
}

juce::ValueTree timbreSpaceReprToVT(std::vector<FeatureContainer<EventwiseStatisticsF>> const &fullTimbreSpace,
                                           std::vector<Real> const &normalizedOnsets,
                                           const juce::String& waveformHash,
                                           const juce::String& audioAbsPath){
    ValueTree vt(axiom::tsn::TimbreAnalysis);
    {
        ValueTree md(axiom::tsn::Metadata);
        md.setProperty(axiom::tsn::Version, LIB_VERSION, nullptr);
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
            for (auto feature : util::Iterator<Feature_e, static_cast<Feature_e>(NumBFCC), Feature_e::f0>()) {
                ValueTree featureTree(toString(feature));
                addEventwiseStatistics(featureTree, timbreFrame[feature]);
                frameTree.addChild(featureTree, -1, nullptr);
            }

            timbreMeasurements.addChild(frameTree, frameIdx, nullptr);

            vt.addChild(timbreMeasurements, 1, nullptr);
        }
    }

    return vt;
}

std::vector<FeatureContainer<EventwiseStatisticsF>> valueTreeToTimbreSpace(juce::ValueTree const &vt)
{
    using namespace analysis;

    std::vector<FeatureContainer<EventwiseStatisticsF>> timbreSpace;

    auto timbreMeasurements = vt.getChildWithName(axiom::tsn::TimbreMeasurements);
    if (!timbreMeasurements.isValid())
        return timbreSpace;

    timbreSpace.reserve(timbreMeasurements.getNumChildren());

    static_assert(static_cast<Feature_e>(0) == Feature_e::bfcc0); // we will be casting ints to Features for the BFCCs
    static_assert(static_cast<Feature_e>(12) == Feature_e::bfcc12);
    for (int frameIdx = 0; frameIdx < timbreMeasurements.getNumChildren(); ++frameIdx)
    {
        auto frameTree = timbreMeasurements.getChild(frameIdx);
        FeatureContainer<EventwiseStatisticsF> frame;

        // extract BFCCs
        if (auto bfccsTree = frameTree.getChildWithName(axiom::tsn::BFCCs);
            bfccsTree.isValid())
        {
            for (int bfccIdx = 0; bfccIdx < bfccsTree.getNumChildren(); ++bfccIdx)
            {
                auto bfccTree = bfccsTree.getChild(bfccIdx);
                frame[static_cast<Feature_e>(bfccIdx)] = (toEventwiseStatistics(bfccTree));
            }
        }

        // extract single-value features
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

std::vector<Real> valueTreeToNormalizedOnsets(juce::ValueTree const &vt)
{
    std::vector<Real> normalizedOnsets;

    const auto onsetArray = vt.getProperty(axiom::tsn::NormalizedOnsets);
    if (!onsetArray.isArray()) {
        return normalizedOnsets;
    }
    auto* array = onsetArray.getArray();
    if (!array) {
        return normalizedOnsets;
    }

    normalizedOnsets.reserve(array->size());

    for (auto && e : *array)
    {
        normalizedOnsets.push_back(static_cast<Real>(e));
    }

    return normalizedOnsets;
}


// Internal template that works with any container
template<typename Container>
[[nodiscard]]
std::vector<Real>
extractFeaturesFromTreeImpl(const juce::ValueTree &frameTree,
                             const Container &featuresToUse,
                             const Statistic statisticToUse)
{
    using namespace analysis;
    std::vector<Real> out;

    if constexpr (requires { featuresToUse.size(); }) {
        out.reserve(featuresToUse.size());
    }

    juce::String statPropName;
    switch (statisticToUse) {
       case Statistic::Mean:     statPropName = axiom::tsn::mean;     break;
       case Statistic::Median:   statPropName = axiom::tsn::median;   break;
       case Statistic::Variance: statPropName = axiom::tsn::variance; break;
       case Statistic::Skewness: statPropName = axiom::tsn::skewness; break;
       case Statistic::Kurtosis: statPropName = axiom::tsn::kurtosis; break;
       default: jassertfalse;
    }

    for (auto f : featuresToUse) {
       const int idx = static_cast<int>(f);
       Real value = 0.0f;

       if (0 <= idx && idx < NumBFCC) {
          const auto bfccsTree = frameTree.getChildWithName(axiom::tsn::BFCCs);
          if (bfccsTree.isValid() && idx < bfccsTree.getNumChildren()) {
             auto bfccTree = bfccsTree.getChild(idx);
             value = bfccTree.getProperty(statPropName, 0.0f);
          }
       }
       else {
          juce::String childName;
          switch (f) {
            case Feature_e::SpectralCentroid:    childName = axiom::tsn::SpectralCentroid; break;
            case Feature_e::SpectralDecrease:    childName = axiom::tsn::SpectralDecrease; break;
            case Feature_e::SpectralFlatness:    childName = axiom::tsn::SpectralFlatness; break;
            case Feature_e::SpectralCrest:       childName = axiom::tsn::SpectralCrest; break;
            case Feature_e::SpectralComplexity:  childName = axiom::tsn::SpectralComplexity; break;
            case Feature_e::StrongPeak:          childName = axiom::tsn::StrongPeak;  break;
            case Feature_e::Periodicity:         childName = axiom::tsn::Periodicity; break;
            case Feature_e::Loudness:            childName = axiom::tsn::Loudness;    break;
            case Feature_e::f0:                  childName = axiom::tsn::f0;          break;
            default: jassertfalse;
          }

            if (auto scalarTree = frameTree.getChildWithName(childName);
                scalarTree.isValid())
            {
                value = scalarTree.getProperty(statPropName, 0.0f);
            }
       }

       out.push_back(value);
    }

    return out;
}

std::vector<Real>
extractFeaturesFromTree(const juce::ValueTree &frameTree,
                        const std::vector<Feature_e> &featuresToUse,
                        const Statistic statisticToUse)
{
    return extractFeaturesFromTreeImpl(frameTree, featuresToUse, statisticToUse);
}

// Overload for single feature - wraps it in a std::array for iteration
std::vector<Real>
extractFeaturesFromTree(const juce::ValueTree &frameTree,
                        const Feature_e featureToUse,
                        const Statistic statisticToUse)
{
    const std::array features{featureToUse};
    return extractFeaturesFromTreeImpl(frameTree, features, statisticToUse);
}


} // namespace nvs::analysis
