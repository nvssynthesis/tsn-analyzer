//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include "Features.h"
#include "AnalysisUsing.h"

namespace nvs::analysis {

typedef nvs::util::Iterator<Feature_e, Feature_e::bfcc0, Feature_e::f0> FeaturesIterator;   // NOLINT nvs should precede the otherwise-generic util namespace

inline Feature_e toFeature(const std::string_view name) {
    for (const auto f : FeaturesIterator()) {
        if (getFeatureName(f) == name) {
            return f;
        }
    }
    jassertfalse;
    return Feature_e::NumFeatures;
}

inline String toString(const Feature_e f) {
    return String(getFeatureName(f));
}

inline StringArray getFeaturesStringArray() {
    static const StringArray featuresStringArray =
        [](){
            StringArray a;
            for (const auto f : FeaturesIterator()) {
                a.add(toString(f));
            }
            return a;
        }();
    return featuresStringArray;
}

}	// namespace nvs::analysis
