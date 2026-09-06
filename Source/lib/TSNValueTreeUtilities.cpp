//
// Created by Nicholas Solem on 1/31/26.
//

#include "TSNValueTreeUtilities.h"

#include "Analyzer.h"
#include "version.h"

namespace nvs::analysis {

bool validateAnalysisVT(const ValueTree &analysisSuperVT) {
    if (const auto timbreAnalysisTree = analysisSuperVT.getChildWithName(nvs::axiom::tsn::TimbreAnalysis);
        !timbreAnalysisTree.isValid())
    {
        Logger::writeToLog("analysis file tree invalid");
        return false;
    }
    if (const auto metadataTree = analysisSuperVT.getChildWithName(nvs::axiom::tsn::Metadata);
        !metadataTree.isValid())
    {
        Logger::writeToLog("analysis file tree invalid");
        return false;
    }
    return true;
}

ValueTree makeSuperTree(const ValueTree &timbreSpaceTree,
    const String &sampleFilePath,
    const double sampleRate,
    const String &waveformHash,
    const String &settingsHash,
    const ValueTree &settingsTree)
{
    ValueTree analysisSuperVT(axiom::tsn::super);

    /* metadata needs:
     -audio sample absolute path (for loading audio file when analysis is imported)
     -audio file sample rate?
     -settings hash (to quickly confirm that analysis has/has not been done for a given analysisSettings on a given
     audio file) -audio wave hash (for confirming that the analysis is definitely relevant for a given audio file (e.g.
     if the audio gets analyzed, but then is later edited, this will require new analysis)) -later: maybe the settings
     themselves, which would allow to load analysis file and populate the settings of the plugin instance?
    */
    auto metaDataTree = analysisSuperVT.getOrCreateChildWithName(axiom::tsn::Metadata, nullptr);
    metaDataTree.setProperty(axiom::tsn::Version, LIB_VERSION, nullptr);
    metaDataTree.setProperty(axiom::tsn::CreationTime, juce::Time::getCurrentTime().toString(true, true, true, true), nullptr);
    metaDataTree.setProperty(axiom::tsn::sampleFilePath, sampleFilePath, nullptr);
    metaDataTree.setProperty(axiom::tsn::sampleRate, sampleRate, nullptr);
    metaDataTree.setProperty(axiom::tsn::audioHash, waveformHash, nullptr);
    metaDataTree.setProperty(axiom::tsn::settingsHash, settingsHash, nullptr);

    metaDataTree.addChild(settingsTree.createCopy(), -1, nullptr);

    analysisSuperVT.addChild(metaDataTree, -1, nullptr);
    analysisSuperVT.addChild(timbreSpaceTree, -1, nullptr);

    return analysisSuperVT;
}

void addEventwiseStatistics(ValueTree& tree, const EventwiseStatisticsF& stats) {
    tree.setProperty(axiom::tsn::mean, stats.mean, nullptr);
    tree.setProperty(axiom::tsn::median, stats.median, nullptr);
    tree.setProperty(axiom::tsn::variance, stats.variance, nullptr);
    tree.setProperty(axiom::tsn::skewness, stats.skewness, nullptr);
    tree.setProperty(axiom::tsn::kurtosis, stats.kurtosis, nullptr);
}

EventwiseStatisticsF toEventwiseStatistics(ValueTree const &vt){
    return {
        .mean = vt.getProperty(axiom::tsn::mean),
        .median = vt.getProperty(axiom::tsn::median),
        .variance = vt.getProperty(axiom::tsn::variance),
        .skewness = vt.getProperty(axiom::tsn::skewness),
        .kurtosis = vt.getProperty(axiom::tsn::kurtosis)
    };
}

namespace {
// BFCC and ACBFCC are the only multi-coefficient Feature_e families -- both get their own nested
// ValueTree subtree (groupName, e.g. "BFCCs"/"ACBFCCs") of per-coefficient children (itemPrefix + index,
// e.g. "BFCC0".."BFCC12") instead of the flat per-feature node every other feature gets.
void writeCoefficientGroup(ValueTree &frameTree, const String &groupName, const String &itemPrefix,
                            std::span<const EventwiseStatisticsF> coeffs) {
    ValueTree groupTree(groupName);
    for (int i = 0; i < static_cast<int>(coeffs.size()); ++i) {
        ValueTree itemTree(itemPrefix + String(i));
        addEventwiseStatistics(itemTree, coeffs[i]);
        groupTree.addChild(itemTree, i, nullptr);
    }
    frameTree.addChild(groupTree, -1, nullptr);
}

void readCoefficientGroup(FeatureContainer<EventwiseStatisticsF> &features, const ValueTree &frameTree,
                           const String &groupName, const Feature_e firstCoeff) {
    const auto groupTree = frameTree.getChildWithName(groupName);
    if (!groupTree.isValid()) return;
    for (int i = 0; i < groupTree.getNumChildren(); ++i) {
        features[static_cast<Feature_e>(static_cast<int>(firstCoeff) + i)] =
            toEventwiseStatistics(groupTree.getChild(i));
    }
}
} // anonymous namespace

juce::String getPacmapDimName (const int d) {
    return axiom::tsn::PaCMAP + juce::String(d);
};

vecReal timbreAnalysisValueTreeToOnsets(const ValueTree &vt) {
    jassert(vt.hasType(axiom::tsn::TimbreAnalysis));
    const auto normOnsetsVar = vt.getProperty(axiom::tsn::NormalizedOnsets);
    jassert(normOnsetsVar.isArray());
    const auto normOnsetsArr = normOnsetsVar.getArray();

    const std::vector<float> retval(normOnsetsArr->begin(), normOnsetsArr->end());
    jassert(retval.size() == normOnsetsArr->size());

    return retval;
}
vecVecReal timbreAnalysisValueTreeToPacmapMatrix(const ValueTree &vt) {
    jassert(vt.hasType(axiom::tsn::TimbreAnalysis));
    const auto timbreMeasurementsVT = vt.getChildWithName(axiom::tsn::TimbreMeasurements);
    const auto pacmapVT = timbreMeasurementsVT.getChildWithName(axiom::tsn::PaCMAP);
    const auto numDim = pacmapVT.getNumProperties();

    vecVecReal retval;
    retval.reserve(numDim);
    for (int i = 0; i < numDim; ++i) {
        const auto pacmapDimVar = pacmapVT.getProperty(getPacmapDimName(i));
        jassert(pacmapDimVar.isArray());
        const auto pacmapArr = pacmapDimVar.getArray();
        retval.emplace_back(pacmapArr->begin(), pacmapArr->end());
    }
    jassert(retval.size() == numDim);
    return retval;
}
std::vector<FeatureContainer<EventwiseStatisticsF>> timbreAnalysisValueTreeToTimbreSpaceRepr(const ValueTree &vt) {
    jassert(vt.hasType(axiom::tsn::TimbreAnalysis));
    const auto timbreMeasurementsVT = vt.getChildWithName(axiom::tsn::TimbreMeasurements);
    const auto numFrames = timbreMeasurementsVT.getNumChildren();
    std::vector<FeatureContainer<EventwiseStatisticsF>> retval;
    retval.reserve(numFrames);

    for (const auto frame : timbreMeasurementsVT) {
        jassert (frame.hasType(axiom::tsn::Frame));
        FeatureContainer<EventwiseStatisticsF> features;

        // fill features with proper stats
        for (const auto feature : FeaturesIterator()) {
            if (isBFCC(feature) || isACBFCC(feature)) continue;
            const auto frameFeat = frame.getChildWithName(toString(feature));
            // get stats and fill them into features
            features[feature].mean = frameFeat.getProperty(axiom::tsn::mean);
            features[feature].median = frameFeat.getProperty(axiom::tsn::median);
            features[feature].variance = frameFeat.getProperty(axiom::tsn::variance);
            features[feature].skewness = frameFeat.getProperty(axiom::tsn::skewness);
            features[feature].kurtosis = frameFeat.getProperty(axiom::tsn::kurtosis);
        }
        readCoefficientGroup(features, frame, axiom::tsn::BFCCs, Feature_e::bfcc0);
        readCoefficientGroup(features, frame, axiom::tsn::ACBFCCs, Feature_e::acbfcc0);

        retval.push_back(features);
    }
    jassert(retval.size() == numFrames);
    return retval;
}

ValueTree timbreSpaceReprToVT(
    std::vector<FeatureContainer<EventwiseStatisticsF>> const &fullTimbreSpace,
    vecReal const &normalizedOnsets,
    vecVecReal const *pacmapMatrix)
{
    ValueTree vt(axiom::tsn::TimbreAnalysis);
    {
        var onsetArray;
        for (auto const &o : normalizedOnsets) {
            onsetArray.append(o);
        }
        vt.setProperty(axiom::tsn::NormalizedOnsets, onsetArray, nullptr);
    }
    {
        ValueTree timbreMeasurements(axiom::tsn::TimbreMeasurements);

        for (int frameIdx = 0; frameIdx < static_cast<int>(fullTimbreSpace.size()); ++frameIdx){
            const auto &timbreFrame = fullTimbreSpace[frameIdx];

            ValueTree frameTree(axiom::tsn::Frame);

            writeCoefficientGroup(frameTree, axiom::tsn::BFCCs, axiom::tsn::BFCC, timbreFrame.bfccs());
            writeCoefficientGroup(frameTree, axiom::tsn::ACBFCCs, axiom::tsn::ACBFCC, timbreFrame.acbfccs());

            // add single-value features
            for (const auto feature : FeaturesIterator()) {
                if (isBFCC(feature) || isACBFCC(feature)) continue;
                ValueTree featureTree(toString(feature));
                addEventwiseStatistics(featureTree, timbreFrame[feature]);
                frameTree.addChild(featureTree, -1, nullptr);
            }

            timbreMeasurements.addChild(frameTree, frameIdx, nullptr);

            vt.addChild(timbreMeasurements, 1, nullptr);
        }
    }
    {
        ValueTree pacmap(axiom::tsn::PaCMAP);
        if (pacmapMatrix == nullptr || pacmapMatrix->empty()) {
            // just write a blank subtree
            vt.addChild(pacmap, -1, nullptr);
        }
        else {
            jassert(pacmapMatrix != nullptr);
            const auto pacmapMatrixDimensionwise = transpose(*pacmapMatrix);
            const auto numDim = pacmapMatrixDimensionwise.size();
            for (int dim = 0; dim < numDim; ++dim) {
                var pacmapDimVar;
                const auto &pacmapDimVec = pacmapMatrixDimensionwise[dim];
                for (int i = 0; i < pacmapDimVec.size(); ++i) {
                    pacmapDimVar.append(pacmapDimVec[i]);
                }
                jassert(pacmapDimVar.size() == pacmapDimVec.size());
                pacmap.setProperty(getPacmapDimName(dim), pacmapDimVar, nullptr);
            }
            vt.addChild(pacmap, -1, nullptr);
        }
    }
    return vt;
}

std::vector<FeatureContainer<EventwiseStatisticsF>> valueTreeToTimbreSpace(ValueTree const &vt)
{
    using namespace analysis;

    std::vector<FeatureContainer<EventwiseStatisticsF>> timbreSpace;

    auto timbreMeasurements = vt.getChildWithName(axiom::tsn::TimbreMeasurements);
    if (!timbreMeasurements.isValid())
        return timbreSpace;

    timbreSpace.reserve(timbreMeasurements.getNumChildren());

    for (int frameIdx = 0; frameIdx < timbreMeasurements.getNumChildren(); ++frameIdx)
    {
        auto frameTree = timbreMeasurements.getChild(frameIdx);
        FeatureContainer<EventwiseStatisticsF> frame;

        readCoefficientGroup(frame, frameTree, axiom::tsn::BFCCs, Feature_e::bfcc0);
        readCoefficientGroup(frame, frameTree, axiom::tsn::ACBFCCs, Feature_e::acbfcc0);

        // extract single-value features
        for (auto const feature : FeaturesIterator()) {
            if (isBFCC(feature) || isACBFCC(feature)) continue;
            if (auto featureTree = frameTree.getChildWithName(toString(feature));
                featureTree.isValid())
            {
                frame[feature] = toEventwiseStatistics(featureTree);
            }
        }

        timbreSpace.push_back(frame);
    }

    return timbreSpace;
}

std::vector<Real> valueTreeToNormalizedOnsets(ValueTree const &vt)
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
std::vector<Real> extractFeaturesFromTreeImpl(const ValueTree &frameTree,
                             const Container &featuresToUse,
                             const Statistic statisticToUse)
{
    using namespace analysis;
    std::vector<Real> out;

    if constexpr (requires { featuresToUse.size(); }) {
        out.reserve(featuresToUse.size());
    }

    String statPropName;
    switch (statisticToUse) {
       case Statistic::Mean:     statPropName = axiom::tsn::mean;     break;
       case Statistic::Median:   statPropName = axiom::tsn::median;   break;
       case Statistic::Variance: statPropName = axiom::tsn::variance; break;
       case Statistic::Skewness: statPropName = axiom::tsn::skewness; break;
       case Statistic::Kurtosis: statPropName = axiom::tsn::kurtosis; break;
       default: jassertfalse;
    }

    for (auto f : featuresToUse) {
       Real value = 0.0f;

       if (isBFCC(f) || isACBFCC(f)) {
          const auto groupName = isBFCC(f) ? axiom::tsn::BFCCs : axiom::tsn::ACBFCCs;
          const auto firstCoeff = isBFCC(f) ? Feature_e::bfcc0 : Feature_e::acbfcc0;
          const int localIdx = static_cast<int>(f) - static_cast<int>(firstCoeff);
          const auto groupTree = frameTree.getChildWithName(groupName);
          if (groupTree.isValid() && localIdx < groupTree.getNumChildren()) {
             auto coeffTree = groupTree.getChild(localIdx);
             value = coeffTree.getProperty(statPropName, 0.0f);
          }
       }
       else {
          String childName;
          childName = toString(f);

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

std::vector<Real> extractFeaturesFromTree(const ValueTree &frameTree,
                        const std::vector<Feature_e> &featuresToUse,
                        const Statistic statisticToUse)
{
    return extractFeaturesFromTreeImpl(frameTree, featuresToUse, statisticToUse);
}

// Overload for single feature - wraps it in a std::array for iteration
std::vector<Real> extractFeaturesFromTree(const ValueTree &frameTree,
                        const Feature_e featureToUse,
                        const Statistic statisticToUse)
{
    const std::array features{featureToUse};
    return extractFeaturesFromTreeImpl(frameTree, features, statisticToUse);
}


} // namespace nvs::analysis
