/*
==============================================================================

    ModernSettings.cpp
    Created: 21 May 2026 12:51:48am
    Author:  Nicholas Solem

  ==============================================================================
*/
#include <string_view>

#include "ModernSettings.h"
#include "StringAxiom.h"

namespace nvs::analysis::modern {
namespace ax = axiom::tsn;

template<StringLiteral Str>
struct WindowType {
    static constexpr std::string_view value = Str.view();
};

using Hann_t = WindowType<ax::hann>;
using Hamming_t = WindowType<ax::hamming>;
using Hannnsgcq_t = WindowType<ax::hannnsgcq>;
using Triangular_t = WindowType<ax::triangular>;
using Square_t = WindowType<ax::square>;
using Blackmanharris62_t = WindowType<ax::blackmanharris62>;
using Blackmanharris70_t = WindowType<ax::blackmanharris70>;
using Blackmanharris74_t = WindowType<ax::blackmanharris74>;
using BlackmanHarris92_t = WindowType<ax::blackmanharris92>;

// settings groups
using AnalysisSettings = SettingsGroup<ax::Analysis,
    RangedSetting<int, 1024, ax::frameSize, 64, 8192>,
    RangedSetting<int, 512, ax::hopSize, 32, 4096>,
    ChoiceSetting<ax::blackmanharris92, ax::windowingType,
        ax::hann, ax::hamming, ax::hannnsgcq, ax::triangular, ax::square,
        ax::blackmanharris62, ax::blackmanharris70,
        ax::blackmanharris74, ax::blackmanharris74>,
    RangedSetting<int, 2, ax::numThreads, 1, 16>
>;

using SpectralComplexitySettings = SettingsGroup<ax::SpectralComplexity,
    RangedSetting<double, 0.005, ax::magnitudeThreshold, 0.0, 1.0>
>;

using SpectralPeakSettings = SettingsGroup<ax::SpectralPeak,
    RangedSetting<double, -60.0, ax::magnitudeThreshold_dB, -100.0, 0.0>,
    RangedSetting<double, 40.0, ax::minFrequency, 0.0, 1000.0>,
    RangedSetting<double, 6000.0, ax::maxFrequency, 1000.0, 20000.0>,
    RangedSetting<int, 64, ax::maxPeaks, 1, 128>
>;

using ExampleRegistry = SettingsRegistry<
    AnalysisSettings,
    SpectralComplexitySettings,
    SpectralPeakSettings
>;

// Bridge to legacy system - can be used while transitioning
struct ModernToLegacyBridge {
    static std::map<juce::String, AnySpec> createAnalysisSpecs() {
        return AnalysisSettings::getSpecs();
    }
    
    static std::map<juce::String, AnySpec> createSpectralComplexitySpecs() {
        return SpectralComplexitySettings::getSpecs();
    }
    
    static std::map<juce::String, AnySpec> createSpectralPeakSpecs() {
        return SpectralPeakSettings::getSpecs();
    }
};

} // namespace nvs::analysis::modern
