
/*
  ==============================================================================

    Features.h
    Created: 2 Jul 2025 8:35:17pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <span>
#include <set>
#include <array>
#include "StringAxiom.h"

namespace nvs::analysis {

// X-macro definition for all features
#define FEATURE_LIST(X) \
    X(bfcc0, axiom::tsn::BFCC0, axiom::tsn::BFCC, "", true) \
    X(bfcc1, axiom::tsn::BFCC1, axiom::tsn::BFCC, "", true) \
    X(bfcc2, axiom::tsn::BFCC2, axiom::tsn::BFCC, "", true) \
    X(bfcc3, axiom::tsn::BFCC3, axiom::tsn::BFCC, "", true) \
    X(bfcc4, axiom::tsn::BFCC4, axiom::tsn::BFCC, "", true) \
    X(bfcc5, axiom::tsn::BFCC5, axiom::tsn::BFCC, "", true) \
    X(bfcc6, axiom::tsn::BFCC6, axiom::tsn::BFCC, "", true) \
    X(bfcc7, axiom::tsn::BFCC7, axiom::tsn::BFCC, "", true) \
    X(bfcc8, axiom::tsn::BFCC8, axiom::tsn::BFCC, "", true) \
    X(bfcc9, axiom::tsn::BFCC9, axiom::tsn::BFCC, "", true) \
    X(bfcc10, axiom::tsn::BFCC10, axiom::tsn::BFCC, "", true) \
    X(bfcc11, axiom::tsn::BFCC11, axiom::tsn::BFCC, "", true) \
    X(bfcc12, axiom::tsn::BFCC12, axiom::tsn::BFCC, "", true) \
    X(SpectralCentroid,     axiom::tsn::SpectralCentroid,   axiom::tsn::spectral, "barks", true) \
    X(SpectralDecrease,     axiom::tsn::SpectralDecrease,   axiom::tsn::spectral, "", true) \
    X(SpectralFlatness,     axiom::tsn::SpectralFlatness,   axiom::tsn::spectral, "", true) \
    X(SpectralCrest,        axiom::tsn::SpectralCrest,      axiom::tsn::spectral, "", true) \
    X(SpectralComplexity,   axiom::tsn::SpectralComplexity, axiom::tsn::spectral, "", true) \
    X(StrongPeak,           axiom::tsn::StrongPeak,         axiom::tsn::spectral, "", true) \
    X(PitchSalience,        axiom::tsn::PitchSalience,      axiom::tsn::spectral, "", true) \
    X(Inharmonicity,        axiom::tsn::Inharmonicity,      axiom::tsn::spectral, "", true) \
    X(Roughness,            axiom::tsn::Roughness,          axiom::tsn::spectral, "", true) \
    X(NoisinessAggregate,   axiom::tsn::NoisinessAggregate, axiom::tsn::spectral, "", true) \
    X(Periodicity,          axiom::tsn::Periodicity,        axiom::tsn::pitch, "", false) \
    X(Loudness,             axiom::tsn::Loudness,           axiom::tsn::loudness, "", false) \
    X(ZwickerLoudness,      axiom::tsn::ZwickerLoudness,    axiom::tsn::loudness, "sone", false) \
    X(acbfcc0,  axiom::tsn::ACBFCC0,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc1,  axiom::tsn::ACBFCC1,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc2,  axiom::tsn::ACBFCC2,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc3,  axiom::tsn::ACBFCC3,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc4,  axiom::tsn::ACBFCC4,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc5,  axiom::tsn::ACBFCC5,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc6,  axiom::tsn::ACBFCC6,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc7,  axiom::tsn::ACBFCC7,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc8,  axiom::tsn::ACBFCC8,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc9,  axiom::tsn::ACBFCC9,  axiom::tsn::ACBFCC, "", false) \
    X(acbfcc10, axiom::tsn::ACBFCC10, axiom::tsn::ACBFCC, "", false) \
    X(acbfcc11, axiom::tsn::ACBFCC11, axiom::tsn::ACBFCC, "", false) \
    X(acbfcc12, axiom::tsn::ACBFCC12, axiom::tsn::ACBFCC, "", false) \
    X(f0,                   axiom::tsn::f0,                 axiom::tsn::pitch, "Hz", false)

// auto-generate enum
enum class Feature_e {
#define ENUM_ENTRY(name, ...) name,
    FEATURE_LIST(ENUM_ENTRY)
#undef ENUM_ENTRY
    NumFeatures
};

// auto-generate metadata
struct FeatureInfo {
    const char* name;
    const char* category;
    const char* unit;
    // sharesTimbralFrameLoop is an implementation detail, not a perceptual claim: it means this feature
    // is computed inside calculateTimbres's shared STFT frame loop (TimbreAnalysis.cpp) and therefore
    // joins that loop's single batch reduction to EventwiseStats in calculateEventwiseTimbreDescription.
    // Pitch/loudness-family features (including ZwickerLoudness and its derived ACBFCCs) are computed
    // by their own dedicated functions on their own frame timebases, so this is false for them even
    // though some (e.g. ACBFCC) are just as "timbral" in the perceptual sense as BFCC is.
    bool sharesTimbralFrameLoop;
};

inline constexpr std::array<FeatureInfo, static_cast<size_t>(Feature_e::NumFeatures)> FeatureRegistry {{
#define REGISTRY_ENTRY(name, displayName, category, unit, sharesTimbralFrameLoop) {displayName, category, unit, sharesTimbralFrameLoop},
    FEATURE_LIST(REGISTRY_ENTRY)
#undef REGISTRY_ENTRY
}};

// constants derived from registry
static constexpr int NumBFCC = 13;
static constexpr int NumACBFCC = 13;
static constexpr auto NumTimbralFeatures = []() {
    int count = 0;
    for (const auto& info : FeatureRegistry) {
        if (info.sharesTimbralFrameLoop) ++count;
    }
    return count;
}();
// Periodicity, Loudness, ZwickerLoudness, 13x acbfcc, f0
static_assert(NumTimbralFeatures == static_cast<int>(Feature_e::NumFeatures) - 17);

namespace {
constexpr int lastTimbralFeatureIdx = []() {
    int last = -1;
    for (int i = 0; i < static_cast<int>(Feature_e::NumFeatures); ++i) {
        if (FeatureRegistry[i].sharesTimbralFrameLoop) {
            last = i;
        }
    }
    return last;
}();
}
static_assert(lastTimbralFeatureIdx == NumTimbralFeatures - 1,
    "sharesTimbralFrameLoop features must form a contiguous prefix, preceding every other feature");

// utility functions
constexpr const char* getFeatureName(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].name;
}

constexpr const char* getFeatureCategory(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].category;
}

constexpr bool featureSharesTimbralFrameLoop(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].sharesTimbralFrameLoop;
}

constexpr bool isBFCC(const Feature_e f) {
    return getFeatureCategory(f) == axiom::tsn::BFCC;
}

constexpr bool isACBFCC(const Feature_e f) {
    return getFeatureCategory(f) == axiom::tsn::ACBFCC;
}


// legacy compatibility
const std::set bfccSet {
#define BFCC_SET_ENTRY(name, displayName, category, unit, isTimbral) \
    std::conditional_t<std::string_view(category) == axiom::tsn::BFCC, Feature_e, void>{Feature_e::name},
    // creates syntax error for non-BFCC entries, effectively filtering
#define BFCC_ONLY(name, displayName, category, unit, isTimbral) \
    Feature_e::name,
	Feature_e::bfcc0,
    Feature_e::bfcc1, Feature_e::bfcc2,
    Feature_e::bfcc3, Feature_e::bfcc4, Feature_e::bfcc5,
    Feature_e::bfcc6, Feature_e::bfcc7, Feature_e::bfcc8,
    Feature_e::bfcc9, Feature_e::bfcc10,
    Feature_e::bfcc11, Feature_e::bfcc12
};

template <typename T>	// T can foreseeably be either single Real, vecReal, or EventwiseStatistics
struct FeatureContainer {
    std::array<T, static_cast<size_t>(Feature_e::NumFeatures)> features {};

    T& operator[](Feature_e f) { return features[static_cast<size_t>(f)]; }
    const T& operator[](Feature_e f) const { return features[static_cast<size_t>(f)]; }

    std::span<T> bfccs() { return {features.data(), NumBFCC}; }
    std::span<const T> bfccs() const { return {features.data(), NumBFCC}; }

    std::span<T> acbfccs() { return {features.data() + static_cast<size_t>(Feature_e::acbfcc0), NumACBFCC}; }
    std::span<const T> acbfccs() const { return {features.data() + static_cast<size_t>(Feature_e::acbfcc0), NumACBFCC}; }
};

inline void pushBFCCFrame(FeatureContainer<std::vector<float>>& container, const std::span<const float> bfccFrame) {
    assert(bfccFrame.size() == NumBFCC);
    for (size_t i = 0; i < NumBFCC; ++i) {
        container[static_cast<Feature_e>(i)].push_back(bfccFrame[i]);
    }
}

}	// namespace nvs::analysis