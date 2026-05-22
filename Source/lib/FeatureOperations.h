//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include "Features.h"

namespace nvs::analysis {

typedef nvs::util::Iterator<Feature_e, Feature_e::bfcc0, Feature_e::f0> featuresIterator;

inline Feature_e toFeature(const std::string_view name) {
    for (const auto f : featuresIterator()) {
        if (getFeatureName(f) == name) {
            return f;
        }
    }
    jassertfalse;
    return Feature_e::NumFeatures;
}

inline juce::String toString(const Feature_e f) {
    return juce::String(getFeatureName(f));
}

}	// namespace nvs::analysis
