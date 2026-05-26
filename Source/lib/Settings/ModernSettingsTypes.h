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
    RangedSetting<SInfo<ax::numThreads,
        "The number of threads used for timbral analysis. Higher # of threads can lead to faster analysis, but limited testing has been done for greater than 1 thread.">,
        int, 4, 1, 8>
>;

using BFCCSettings_t = SettingsGroup<ax::BFCC,
    RangedSetting<SInfo<ax::lowFrequencyBound,
        "Lower bound of the frequency range. Bandlimiting to ~500 can help with classification.">,
        double, 500.0, 0.0, 5000.0, "Hz">,
    RangedSetting<SInfo<ax::highFrequencyBound,
        "Upper bound of the frequency range. Bandlimiting to ~4000 can help for classification.">,
        double, 4000.0, 2000.0, 22000.0, "Hz">,
    RangedSetting<SInfo<ax::liftering, "the liftering coefficient. Use '0' to bypass it">, int, 0, 0, 100>,
    RangedSetting<SInfo<ax::numBands, "the number of bark bands in the filter">, int, 40, 1, 128>,
    RangedSetting<SInfo<ax::numCoefficients>, int, 13, 5, 26>,
    ChoiceSetting<
        SInfo<
            ax::normalize,
            "'unit_max' makes the vertex of all the triangles equal to 1, 'unit_sum' makes the area of all the triangles equal to 1."
        >, ax::unit_sum, ax::unit_sum, ax::unit_max>,
    RangedSetting<
        SInfo<
            ax::BFCC0_frameNormalizationFactor,
            "Per frame, normalize the BFCC vector based on the 0th BFCC, which represents overall energy. This adjusts the frame's contribution to the overall event's BFCC calculation."
        >, double, 1.0/20.0, 0.0, 1.0>,
    BoolSetting<
        SInfo<
            ax::BFCC0_eventNormalize,
            "Per event, normalize the BFCC vector based on the 0th BFCC, which represents overall energy. This normalizes the overall event's BFCCs based on the 0th."
        >, true>,
    ChoiceSetting<SInfo<ax::spectrumType, "Whether to use magnitude or power spectrum.">,
        ax::power, ax::magnitude, ax::power>,
    ChoiceSetting<SInfo<ax::weightingType, "type of weighting function for determining triangle area">,
        ax::warping, ax::warping, ax::linear>,
    ChoiceSetting<SInfo<ax::dctType>, ax::typeII, ax::typeII, ax::typeIII>
>;

using OnsetSettings_t = SettingsGroup<ax::Onset,
    ChoiceSetting<SInfo<ax::segmentation, "whether to segment by detected events or uniform frames">,
        ax::Event, ax::Event, ax::Uniform>,
    RangedSetting<SInfo<ax::silenceThreshold, "the threshold for silence in initial onset detection">,
        double, 0.1, 0.0, 1.0>,
    RangedSetting<SInfo<ax::alpha, "the proportion of the mean included to reject smaller peaks; filters very short onsets">,
        double, 0.1, 0.0, 1.0>,
    RangedSetting<SInfo<ax::numFrames_shortOnsetFilter, "the number of frames used to compute the threshold; size of short-onset filter">,
        int, 5, 1, 64>,
    RangedSetting<SInfo<ax::weight_hfc, "the High Frequency Content detection function which accurately detects percussive events">,
        double, 0.0, 0.0, 1.0>,
    RangedSetting<SInfo<ax::weight_complex, "the Complex-Domain spectral difference function taking into account changes in magnitude and phase. It emphasizes note onsets either as a result of significant change in energy in the magnitude spectrum, and/or a deviation from the expected phase values in the phase spectrum, caused by a change in pitch.">,
        double, 0.5, 0.0, 1.0>,
    RangedSetting<SInfo<ax::weight_complexPhase, "the simplified Complex-Domain spectral difference function taking into account phase changes, weighted by magnitude. It reacts better on tonal sounds such as bowed string, but tends to over-detect percussive events.">,
        double, 0.0, 0.0, 1.0>,
    RangedSetting<SInfo<ax::weight_flux, "the Spectral Flux detection function which characterizes changes in magnitude spectrum.">,
        double, 0.0, 0.0, 1.0>,
    RangedSetting<SInfo<ax::weight_rms, "the difference function, measuring the half-rectified change of the RMS of the magnitude spectrum (i.e., measuring overall energy flux)">,
        double, 0.0, 0.0, 1.0>,
    RangedSetting<SInfo<ax::weight_novelty, "the novelty curve function reveals the predominant local pulse. works well with soft onsets.">,
        double, 0.0, 0.0, 1.0>,
    RangedSetting<SInfo<ax::refinementNumEventSubdivisions, "For each detected sound event, subdivide by this amount. Uses a special cumulative energy algorithm rather than equal subdivisions.">,
        int, 1, 1, 25>,
    RangedSetting<SInfo<ax::refinementSilenceThresholdDb, "The silence threshold for the post-onset detection silence detection algorithm, which is used only to create events from silences">,
        double, -50.0, -100.0, 0.0, "dB">,
    RangedSetting<SInfo<ax::refinementMinSilenceDurationMs, "The minimum length for a silence to be counted as such">,
        double, 300.0, 0.0, 5000.0, "ms">,
    RangedSetting<SInfo<ax::refinementMinEventWithinSilenceDurationMs, "The minimum length for an event found within a silence to be counted as such">,
        double, 300.0, 0.0, 5000.0, "ms">
>;

using PitchSettings_t = SettingsGroup<ax::Pitch,
    ChoiceSetting<SInfo<ax::pitchDetectionAlgorithm>,
        ax::yin, ax::yin, ax::yinFFT, ax::pYin, ax::chroma>,
    RangedSetting<SInfo<ax::frameSize, "Recommended: at least 4096 for reliable pitch detection">,
        int, 4096, 64, 16384>,
    RangedSetting<SInfo<ax::hopSize>, int, 2048, 32, 8192>,
    BoolSetting<SInfo<ax::interpolate>, true>,
    RangedSetting<SInfo<ax::maxFrequency>, double, 4000.0, 20.0, 22050.0>,
    RangedSetting<SInfo<ax::minFrequency>, double, 140.0, 20.0, 22050.0>,
    RangedSetting<SInfo<ax::tolerance>, double, 0.15, 0.0, 1.0>,
    BoolSetting<SInfo<ax::replace_dismal_confidences_with_constant>, true>,
    RangedSetting<SInfo<ax::dismal_confidence_threshold,
        "The maximum pitch confidence at which the detected pitch value is allowed to pass without replacement.">,
        double, 0.0, 0.0, 1.0>,
    ChoiceSetting<SInfo<ax::dismal_replacement_constant,
        "The value with which to replace any detected pitches with low confidence.">,
        "negative", "negative", "zero", "nyquist">,
    RangedSetting<SInfo<ax::lowRMSThreshold>, double, 0.1, 0.0, 1.0>,
    BoolSetting<SInfo<ax::preciseTime>, false>
>;

using LoudnessSettings_t = SettingsGroup<ax::Loudness,
    BoolSetting<SInfo<ax::equalizeLoudness>, true>
>;

using PitchSalienceSettings_t = SettingsGroup<ax::PitchSalience,
    RangedSetting<SInfo<ax::highBoundary, "Upper frequency boundary for pitch salience analysis [Hz]">,
        double, 5000.0, 0.0, 22050.0, "Hz">,
    RangedSetting<SInfo<ax::lowBoundary, "Lower frequency boundary for pitch salience analysis [Hz]">,
        double, 100.0, 0.0, 22050.0, "Hz">
>;

using SplitSettings_t = SettingsGroup<ax::Split,
    RangedSetting<SInfo<ax::fadeInSamps>, int, 5, 0, 10000>,
    RangedSetting<SInfo<ax::fadeOutSamps>, int, 5, 0, 10000>
>;

using PaCMAPSettings_t = SettingsGroup<ax::PaCMAP,
    RangedSetting<SInfo<ax::num_neighbours, "Number of nearest neighbors.">,
        int, 15, 5, 35>,
    RangedSetting<SInfo<ax::MN_ratio, "Mid-near pairs (ratio to num nearest neighbors).">,
        double, 0.5, 0.1, 10.0>,
    RangedSetting<SInfo<ax::FP_ratio, "Further points edges (ratio to num nearest neighbors). Increasing FP_ratio pulls global structure apart, which can break string-like collapse">,
        double, 2.0, 0.1, 10.0>,
    RangedSetting<SInfo<ax::learning_rate, "Learning rate.">,
        double, 1.0, 0.1, 10.0>,
    RangedSetting<SInfo<ax::phase_1_iters>, int, 100, 1, 200>,
    RangedSetting<SInfo<ax::phase_2_iters>, int, 100, 1, 200>,
    ChoiceSetting<SInfo<ax::preprocess_mode, "Whether to use range-based normalization or z-score based standardization for initialization.">,
        ax::Normalize, ax::Normalize, ax::Standardize>
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
