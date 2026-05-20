/*
  ==============================================================================

    Settings.h
    Created: 30 Oct 2023 2:15:28pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "essentia/types.h"
#include <juce_data_structures/juce_data_structures.h>

#include "DimensionalityReduction/pca.h"

namespace nvs::analysis {

void ensureBranchAndInitializeDefaults (juce::ValueTree& settingsVT, const juce::String& branchName);
void initializeSettingsBranches(juce::ValueTree& settingsVT, bool dbg=false);
bool verifySettingsStructure (const juce::ValueTree& settingsVT);
bool verifySettingsStructureWithAttemptedFix (juce::ValueTree& settingsVT);

using NormalisableRangeDouble = juce::NormalisableRange<double>;

template<typename T>
struct RangedSettingsSpec
{
    NormalisableRangeDouble range;
    T defaultValue;
    juce::String tooltip = {};           // Optional tooltip
    int numDecimalPlaces = 2;            // Default precision
    juce::String unit = {};              // e.g., "Hz", "dB", "ms"
};
struct ChoiceSettingsSpec
{
    std::vector<juce::String> options;
    juce::String defaultValue;
    juce::String tooltip = {};
};

struct BoolSettingsSpec
{
    bool defaultValue;
    juce::String tooltip = {};
};

using AnySpec = std::variant<
    RangedSettingsSpec<int>,
    RangedSettingsSpec<double>,
    ChoiceSettingsSpec,
    BoolSettingsSpec
>;

// defined in .cpp to avoid circular include (issue was just with calling nvs::analysis::buildFeatureChoiceVec, but this allows consistency)
extern const std::map<juce::String, AnySpec> analysisSpecs, bfccSpecs, onsetSpecs, sBicSpecs, pitchSpecs, splitSpecs, timbreSpaceSpecs;
extern const std::map<juce::String, const std::map<juce::String,AnySpec>*> specsByBranch;

struct AnalyzerSettings {
    struct Analysis {
        double sampleRate = 0.0;
        int frameSize = 1024;
        int hopSize = 1024;
        juce::String windowingType = "hann";
        int numThreads = 2;
    } analysis;

    struct BFCC {
        juce::String dctType = "typeII";
        double highFrequencyBound = 4000.0;
        int liftering = 0;
        double lowFrequencyBound = 500.0;
        juce::String normalize = "unit_sum";
        double BFCC0_frameNormalizationFactor {1.0 / 20.0};
        bool BFCC0_eventNormalize {true};
        int numBands = 40;
        int numCoefficients = 13;
        juce::String spectrumType = "power";
        juce::String weightingType = "warping";
    } bfcc;

    struct SpectralComplexity {
        double magnitudeThreshold = 0.005;
    } spectralComplexity;

    struct Onset {
        enum class Segmentation {
            Event,  // use proper onset detection, making 1 event per onset
            Uniform // use uniformly distributed segments, specified by analysis.hopSize and analysis.frameSize
        } segmentation {Segmentation::Uniform};

        struct Refinement {
            bool doRefinements = false;
            int numEventSubdivisions = 1;   // at least 1
            float silenceThresholdDb = -50.0f;
            float minSilenceDurationMs = 300.0f;
            float minEventWithinSilenceDurationMs = 500.0f;
        } _refinement;

        double alpha = 0.1;
        int numFrames_shortOnsetFilter = 5;
        double silenceThreshold = 0.03125;
        double weight_complex = 0.5;
        double weight_complexPhase = 0.5;
        double weight_flux = 0.5;
        double weight_hfc = 0.5;
        double weight_rms = 0.5;
        double weight_novelty = 0.0;

        double uniform_event_length = 0.150f;    // in seconds
    } onset;

    struct Pitch {
        juce::String pitchDetectionAlgorithm = "yin";   // {yin, pYin, yinFFT} later, think about chroma

        int frameSize = 4096;
        int hopSize = 2048;

        float minimum_confidence_considered {0.6f};  // frames with confidence below this will not be considered in summarizing pitch statistics

        //----------------only for yin----------------
        struct yin {
            double maxFrequency = 3000.0;
            double minFrequency = 100.0;
            bool interpolate = true;    // only for yin
            double tolerance = 0.15;    // only for yin
        } _yin;
        //----------------only for pYin----------------
        struct pYin {
            double lowRMSThreshold = 0.1;
            bool preciseTime = false;
        } _pYin;

        //-------------confidence replacement------------
        bool replace_dismal_confidences_with_constant = true;
        double dismal_confidence_threshold = 0.0; // equal to or below this, corresponding pitch will be considered useless
        float dismal_replacement_constant = -1.0f;
    } pitch;

    struct Loudness {
        bool equalizeLoudness = true;
    } loudness;

    struct Split {
        int fadeInSamps = 5;
        int fadeOutSamps = 5;
    } split;

    struct SBic {
        double complexityPenaltyWeight = 1.5;
        int incrementFirstPass = 60;
        int incrementSecondPass = 20;
        int minSegmentLengthFrames = 10;
        int sizeFirstPass = 300;
        int sizeSecondPass = 200;
    } sBic;

    struct PaCMAP {
        float MN_ratio = 0.5f;
        float FP_ratio = 2.0f;
        float learning_rate = 1.0f;
        int num_neighbours = 15;
        dim::PreprocessMode_e preprocess_mode {dim::PreprocessMode_e::Normalize};
        int phase_1_iters {100};
        int phase_2_iters {100};
    } pacmap;

    struct Info {
        juce::String sampleFilePath;
        juce::String author;
    } info;
};
bool updateSettingsFromValueTree(AnalyzerSettings& settings, const juce::ValueTree& settingsTree);
juce::ValueTree createParentTreeFromSettings(const AnalyzerSettings& settings);

}
