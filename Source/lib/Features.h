
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
    X(bfcc0, "bfcc0", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc1, "bfcc1", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc2, "bfcc2", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc3, "bfcc3", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc4, "bfcc4", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc5, "bfcc5", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc6, "bfcc6", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc7, "bfcc7", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc8, "bfcc8", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc9, "bfcc9", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc10, "bfcc10", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc11, "bfcc11", axiom::tsn::BFCC, "cepstal", true) \
    X(bfcc12, "bfcc12", axiom::tsn::BFCC, "cepstal", true) \
    X(SpectralCentroid, axiom::tsn::SpectralCentroid, "spectral", "barks", true) \
    X(SpectralDecrease, axiom::tsn::SpectralDecrease, "spectral", "", true) \
    X(SpectralFlatness, axiom::tsn::SpectralFlatness, "spectral", "", true) \
    X(SpectralCrest, axiom::tsn::SpectralCrest, "spectral", "", true) \
    X(SpectralComplexity, axiom::tsn::SpectralComplexity, "spectral", "", true) \
    X(StrongPeak, axiom::tsn::StrongPeak, "spectral", "", true) \
    X(PitchSalience, axiom::tsn::PitchSalience, "spectral", "", true) \
    X(Periodicity, axiom::tsn::Periodicity, "pitch", "", false) \
    X(Loudness, axiom::tsn::Loudness, "loudness", "", false) \
    X(f0, axiom::tsn::f0, "pitch", "Hz", false)

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
static_assert(NumTimbralFeatures == 20);

// Utility functions
constexpr const char* getFeatureName(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].name;
}

constexpr const char* getFeatureCategory(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].category;
}

constexpr bool isFeatureTimbral(Feature_e f) {
    return FeatureRegistry[static_cast<size_t>(f)].isTimbral;
}

constexpr bool isBFCC(Feature_e f) {
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