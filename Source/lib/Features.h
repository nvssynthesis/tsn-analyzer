
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
    // the connotation of isTimbral is that all the 'timbral' features get computed in the same series of algorithms, while pitch and loudness do not.
    // Periodicity might be considered a timbral feature (a measure of noisiness), but it should not count as 'isTimbral', because
    // it is computed alongside pitch (which might have a different frame size than the other features).
    bool isTimbral;
};

inline constexpr std::array<FeatureInfo, static_cast<size_t>(Feature_e::NumFeatures)> FeatureRegistry {{
#define REGISTRY_ENTRY(name, displayName, category, unit, isTimbral) {displayName, category, unit, isTimbral},
    FEATURE_LIST(REGISTRY_ENTRY)
#undef REGISTRY_ENTRY
}};

// constants derived from registry
static constexpr int NumBFCC = 13;
static constexpr auto NumTimbralFeatures = []() {
    int count = 0;
    for (const auto& info : FeatureRegistry) {
        if (info.isTimbral) ++count;
    }
    return count;
}();
static_assert(NumTimbralFeatures == static_cast<int>(Feature_e::NumFeatures) - 4); // Periodicity, Loudness, ZwickerLoudness, f0

namespace {
constexpr int lastTimbralFeatureIdx = []() {
    int last = -1;
    for (int i = 0; i < static_cast<int>(Feature_e::NumFeatures); ++i) {
        if (FeatureRegistry[i].isTimbral) {
            last = i;
        }
    }
    return last;
}();
}
static_assert(lastTimbralFeatureIdx < static_cast<int>(Feature_e::Periodicity), "Last timbral feature must precede non-timbral features");
static_assert(lastTimbralFeatureIdx < static_cast<int>(Feature_e::f0), "Last timbral feature must precede non-timbral features");
static_assert(lastTimbralFeatureIdx < static_cast<int>(Feature_e::Loudness), "Last timbral feature must precede non-timbral features");
static_assert(lastTimbralFeatureIdx < static_cast<int>(Feature_e::ZwickerLoudness), "Last timbral feature must precede non-timbral features");

// utility functions
constexpr const char* getFeatureName(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].name;
}

constexpr const char* getFeatureCategory(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].category;
}

constexpr bool isFeatureTimbral(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].isTimbral;
}

constexpr bool isBFCC(const Feature_e f) {
    return getFeatureCategory(f) == axiom::tsn::BFCC;
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
};

inline void pushBFCCFrame(FeatureContainer<std::vector<float>>& container, const std::span<const float> bfccFrame) {
    assert(bfccFrame.size() == NumBFCC);
    for (size_t i = 0; i < NumBFCC; ++i) {
        container[static_cast<Feature_e>(i)].push_back(bfccFrame[i]);
    }
}

}	// namespace nvs::analysis