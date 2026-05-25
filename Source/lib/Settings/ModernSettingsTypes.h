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
    RangedSetting<SInfo<ax::frameSize>, int, 1024, 64, 16384, "Samples">,
    RangedSetting<SInfo<ax::hopSize>, int, 512, 32, 8192, "Samples">,
    ChoiceSetting<SInfo<ax::windowingType, "Use blackmanharris92 for best results, at least for Spectral Peak-based features.">,
        ax::blackmanharris92,
        ax::hann, ax::hamming, ax::hannnsgcq, ax::triangular, ax::square,
        ax::blackmanharris62, ax::blackmanharris70,
        ax::blackmanharris74, ax::blackmanharris74>,
    RangedSetting<SInfo<ax::numThreads>, int, 4, 1, 16>
>;

using BFCCSettings_t = SettingsGroup<ax::BFCC,
    RangedSetting<SInfo<ax::lowFrequencyBound,
        "Lower bound of the frequency range. Bandlimiting to ~500 can help with classification.">,
        double, 500.0, 0.0, 5000.0, "Hz">,
    RangedSetting<SInfo<ax::highFrequencyBound,
        "Upper bound of the frequency range. Bandlimiting to ~4000 can help for classification.">,
        double, 4000.0, 2000.0, 22000.0, "Hz">,
    RangedSetting<SInfo<ax::liftering, "the liftering coefficient. Use '0' to bypass it">, int, 0, 0, 100>,
    RangedSetting<SInfo<ax::numBands, "the number of bark bands in the filter">, int, 40, 1, 128>
// more
>;

using SpectralComplexitySettings_t = SettingsGroup<ax::SpectralComplexity,
    RangedSetting<SInfo<ax::magnitudeThreshold>, double, 0.005, 0.0, 1.0>
>;

using SpectralPeakSettings_t = SettingsGroup<ax::SpectralPeak,
    RangedSetting<SInfo<ax::magnitudeThreshold_dB>, double, -60.0, -100.0, 0.0>,
    RangedSetting<SInfo<ax::minFrequency>, double, 40.0, 0.0, 1000.0>,
    RangedSetting<SInfo<ax::maxFrequency>, double, 6000.0, 1000.0, 20000.0>,
    RangedSetting<SInfo<ax::maxPeaks>, int, 64, 1, 128>
>;

using AnalyzerSettingsRegistry_t = SettingsRegistry<
    AnalysisSettings_t,
    BFCCSettings_t,
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
