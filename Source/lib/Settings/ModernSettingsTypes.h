//
// Created by Nicholas Solem on 5/24/26.
//

#pragma once

#include "ModernSettings.h"
#include "StringAxiom.h"

namespace nvs::analysis::modern {
namespace ax = axiom::tsn;

// settings groups
using AnalysisSettings_t = SettingsGroup<ax::Analysis,
    RangedSetting<int, 1024, ax::frameSize, 64, 16384, "", "Samples">,
    RangedSetting<int, 512, ax::hopSize, 32, 8192, "", "Samples">,
    ChoiceSetting<ax::blackmanharris92, ax::windowingType,
        ax::hann, ax::hamming, ax::hannnsgcq, ax::triangular, ax::square,
        ax::blackmanharris62, ax::blackmanharris70,
        ax::blackmanharris74, ax::blackmanharris74,
        "Use blackmanharris92 for best results, at least for Spectral Peak-based features.">,
    RangedSetting<int, 4, ax::numThreads, 1, 16>
>;

using BFCCSettings_t = SettingsGroup<ax::BFCC,
    RangedSetting<double, 500.0, ax::lowFrequencyBound, 0.0, 5000.0,
    "Lower bound of the frequency range. Bandlimiting to ~500 can help with classification.", "Hz">,
    RangedSetting<double, 4000.0, ax::highFrequencyBound, 2000.0, 22000.0,
        "Upper bound of the frequency range. Bandlimiting to ~4000 can help for classification.", "Hz">,
    RangedSetting<int, 0, ax::liftering, 0, 100, "the liftering coefficient. Use '0' to bypass it">,
    RangedSetting<int, 40, ax::numBands, 1, 128, "the number of bark bands in the filter">
// more
>;

using SpectralComplexitySettings_t = SettingsGroup<ax::SpectralComplexity,
    RangedSetting<double, 0.005, ax::magnitudeThreshold, 0.0, 1.0>
>;

using SpectralPeakSettings_t = SettingsGroup<ax::SpectralPeak,
    RangedSetting<double, -60.0, ax::magnitudeThreshold_dB, -100.0, 0.0>,
    RangedSetting<double, 40.0, ax::minFrequency, 0.0, 1000.0>,
    RangedSetting<double, 6000.0, ax::maxFrequency, 1000.0, 20000.0>,
    RangedSetting<int, 64, ax::maxPeaks, 1, 128>
>;

using ExampleRegistry_t = SettingsRegistry<
    AnalysisSettings_t,
    SpectralComplexitySettings_t,
    SpectralPeakSettings_t
>;

// bridge to legacy system
struct ModernToLegacyBridge {
    static std::map<juce::String, AnySpec> createAnalysisSpecs() {
        return AnalysisSettings_t::getSpecs();
    }

    static std::map<juce::String, AnySpec> createSpectralComplexitySpecs() {
        return SpectralComplexitySettings_t::getSpecs();
    }

    static std::map<juce::String, AnySpec> createSpectralPeakSpecs() {
        return SpectralPeakSettings_t::getSpecs();
    }
};

}
