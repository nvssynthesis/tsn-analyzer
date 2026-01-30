//
// Created by Nicholas Solem on 1/16/26.
//

#pragma once
#include "Features.h"
#include "StringAxiom.h"

namespace nvs::analysis {

typedef nvs::util::Iterator<Feature_e, Feature_e::bfcc0, Feature_e::f0> featuresIterator;

inline juce::String toString(Feature_e f){
    if (static_cast<int>(f) < NumBFCC){
        juce::String s = "bfcc";
        s += static_cast<int>(f);
        return s;
    }
    switch (f) {
        case Feature_e::SpectralCentroid:
            return axiom::tsn::SpectralCentroid;
        case Feature_e::SpectralDecrease:
            return axiom::tsn::SpectralDecrease;
        case Feature_e::SpectralFlatness:
            return axiom::tsn::SpectralFlatness;
        case Feature_e::SpectralCrest:
            return axiom::tsn::SpectralCrest;
        case Feature_e::SpectralComplexity:
            return axiom::tsn::SpectralComplexity;
        case Feature_e::StrongPeak:
            return axiom::tsn::StrongPeak;
        case Feature_e::Periodicity:
            return axiom::tsn::Periodicity;
        case Feature_e::Loudness:
            return axiom::tsn::Loudness;
        case Feature_e::f0:
            return axiom::tsn::f0;
        default:
            jassertfalse;
            return "";
    }
}
inline Feature_e toFeature(const juce::String &s){
    if (s.contains("bfcc")){
        const juce::String intPart = s.removeCharacters("bfcc");
        int i = intPart.getIntValue();
        return static_cast<Feature_e>(i);
    }
    if (s == "Periodicity"){
        return Feature_e::Periodicity;
    }
    else if (s == "Loudness"){
        return Feature_e::Loudness;
    }
    else if (s == "f0"){
        return Feature_e::f0;
    }
    jassertfalse;
    return Feature_e::bfcc0;
}

inline const juce::StringArray& getFeaturesStringArray() {
    static const auto features = [] -> juce::StringArray {
        StringArray result;
        for (const auto f : featuresIterator()) {
            result.add(toString(f));
        }
        return result;
    }();
    return features;
}
}	// namespace nvs::analysis
