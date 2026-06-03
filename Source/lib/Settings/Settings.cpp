/*
  ==============================================================================

    Settings.cpp
    Created: 4 May 2025 2:13:31pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Settings.h"
#include "../Analyzer.h"
#include "../StringAxiom.h"

namespace nvs::analysis::deprecated {

static constexpr bool TIMBRE_SPACE_SETTINGS_EXIST {false};  // these 'settings' were meant to be automatable, so they are now parameters

using ValueTree = juce::ValueTree;

namespace {
template<typename T>
void setTreeProperty(ValueTree& tree, juce::Identifier const& propertyId, T const& value)
{
    tree.setProperty(propertyId, value, nullptr);
}

bool requireChildTree(const ValueTree& parent, juce::Identifier const& childName, ValueTree& child)
{
    child = parent.getChildWithName(childName);
    if (!child.isValid()) {
        std::cerr << childName.toString() << " node missing\n";
        jassertfalse;
        return false;
    }
    return true;
}

bool validateProperties(const ValueTree& node, std::initializer_list<juce::Identifier> properties)
{
    for (auto const& property : properties) {
        if (!node.hasProperty(property)) {
            std::cerr << node.getType().toString() << " node missing property " << property.toString() << "\n";
            jassertfalse;
            return false;
        }
    }
    return true;
}

template<typename T>
bool loadRequiredProperty(const ValueTree& node, juce::Identifier const& propertyName, T& outProperty)
{
    if (!node.hasProperty(propertyName)) {
        std::cerr << node.getType().toString() << " node missing property " << propertyName.toString() << "\n";
        jassertfalse;
        return false;
    }
    outProperty = node.getProperty(propertyName);
    return true;
}


bool loadRequiredProperty(const ValueTree& node, juce::Identifier const& propertyName, juce::String& outProperty)
{
    if (!node.hasProperty(propertyName)) {
        std::cerr << node.getType().toString() << " node missing property " << propertyName.toString() << "\n";
        jassertfalse;
        return false;
    }
    outProperty = node.getProperty(propertyName).toString();
    return true;
}

template<typename... Ts>
bool loadRequiredProperties(const ValueTree& node, std::pair<juce::Identifier, Ts>&&... pairs)
{
    return (loadRequiredProperty(node, pairs.first, pairs.second) && ...);
}
template<typename T>
auto prop(juce::Identifier id, T& out)
{
    return std::pair<juce::Identifier, T&>{ id, out };
}

}

static NormalisableRangeDouble makePowerOfTwoRange (double minValue, double maxValue)
{
    const auto minLog = std::log2 (minValue);
    const auto maxLog = std::log2 (maxValue);
    return {
        minValue, maxValue,
        [=] (double, double, const double n) {
            return std::pow (2.0,
                juce::jmap (n, 0.0, 1.0, minLog, maxLog));
        },
        [=] (double, double, const double v) {
            return juce::jmap (std::log2(v), minLog, maxLog, 0.0, 1.0);
        },
        [] (const double s, const double e, const double v) {
            const auto c = juce::jlimit(s, e, v);
            return std::pow(2.0, std::round(std::log2(c)));
        }
    };
}

/// TODO: add metadata subtree within settings, including author, creation date, and description

const int maxThreads = juce::SystemStats::getNumCpus();
const int defaultThreads = std::max(maxThreads - 2, 1);
const std::map<juce::String, AnySpec> analysisSpecs
{
	{ axiom::tsn::frameSize,     RangedSettingsSpec<int>{   makePowerOfTwoRange(64, 8192), 1024 } }, // NOLINT(readability-redundant-template-arguments)
	{ axiom::tsn::hopSize,       RangedSettingsSpec<int>{   makePowerOfTwoRange(32, 4096),  512 } },
	{ axiom::tsn::windowingType,  ChoiceSettingsSpec{ 	{axiom::tsn::hann, axiom::tsn::hamming, axiom::tsn::hannnsgcq,
		axiom::tsn::triangular, axiom::tsn::square, axiom::tsn::blackmanharris62, axiom::tsn::blackmanharris70,
		axiom::tsn::blackmanharris74, 	axiom::tsn::blackmanharris92},
	    /* default: */axiom::tsn::blackmanharris92,
	    "Use blackmanharris92 for best results, at least for Spectral Peak-based features."} },
    { axiom::tsn::numThreads, RangedSettingsSpec<int>{NormalisableRangeDouble(1, maxThreads), defaultThreads,
        "The number of threads used for timbral analysis. Higher # of threads => faster analysis, but limited testing has been done for greater than 1 thread."}}
};

#pragma message("Pull spectrum settings out from BFCCs, as it is reused by others")
const std::map<juce::String, AnySpec> bfccSpecs
{
	{ axiom::tsn::highFrequencyBound,  RangedSettingsSpec<double>{ {20.0,24000.0,1.0,0.4},4000.0,
	    "Upper bound of the frequency range. Bandlimiting to ~4000 can help for classification.", 1, "Hz"} },
	{ axiom::tsn::lowFrequencyBound,   RangedSettingsSpec<double>{ {0.0,5000.0,1.0,0.4}, 500.0,
	    "Lower bound of the frequency range. Bandlimiting to ~500 can help with classification.", 1, "Hz"} },
	{ axiom::tsn::liftering,           RangedSettingsSpec<int>{   {0,100,1,1 },0,
	    "the liftering coefficient. Use '0' to bypass it"} },
	{ axiom::tsn::numBands,            RangedSettingsSpec<int>{
	    {1,128,1,1},40,
	    "the number of bark bands in the filter" } },
	{ axiom::tsn::numCoefficients,     RangedSettingsSpec<int>{   {5,26,1,1}, 13 } },
	{ axiom::tsn::normalize,           ChoiceSettingsSpec{
	    {axiom::tsn::unit_sum, axiom::tsn::unit_max},axiom::tsn::unit_sum,
	    "'unit_max' makes the vertex of all the triangles equal to 1, 'unit_sum' makes the area of all the triangles equal to 1." } },
    { axiom::tsn::BFCC0_frameNormalizationFactor, RangedSettingsSpec<double>{{0.0, 1.0, 0.0, 1.0}, 1.0/20.0,
        "Per frame, normalize the BFCC vector based on the 0th BFCC, which represents overall energy. This adjusts the frame's contribution to the overall event's BFCC calculation."}},
    { axiom::tsn::BFCC0_eventNormalize, BoolSettingsSpec{true,
        "Per event, normalize the BFCC vector based on the 0th BFCC, which represents overall energy. This normalizes the overall event's BFCCs based on the 0th."}},
    { axiom::tsn::spectrumType,        ChoiceSettingsSpec{ {axiom::tsn::magnitude, axiom::tsn::power},axiom::tsn::power,
	    "Whether to use magnitude or power spectrum."} },
	{ axiom::tsn::weightingType,       ChoiceSettingsSpec{ {axiom::tsn::warping,  axiom::tsn::linear},axiom::tsn::warping,
	    "type of weighting function for determining triangle area"} },
	{ axiom::tsn::dctType,             ChoiceSettingsSpec{ {axiom::tsn::typeII,   axiom::tsn::typeIII},axiom::tsn::typeII } }
};

const std::map<juce::String, AnySpec> onsetSpecs
{
    { axiom::tsn::segmentation, ChoiceSettingsSpec {{axiom::tsn::Event, axiom::tsn::Uniform}, axiom::tsn::Event,
        "whether to segment by detected events or uniform frames"} },
	{ axiom::tsn::silenceThreshold,             RangedSettingsSpec<double>{ {0.0,1.0,0.01,0.4}, 0.1,
	    "the threshold for silence"} },
	{ axiom::tsn::alpha,                        RangedSettingsSpec<double>{ {0.0,1.0,0.01,0.4}, 0.1,
	    "the proportion of the mean included to reject smaller peaks; filters very short onsets" } },
	{ axiom::tsn::numFrames_shortOnsetFilter,   RangedSettingsSpec<int>  { { 1,  64,  1,   1 },    5,
	    "the number of frames used to compute the threshold; size of short-onset filter"} },
	{ axiom::tsn::weight_hfc,                   RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the High Frequency Content detection function which accurately detects percussive events" } },
	{ axiom::tsn::weight_complex,               RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.1,
	    "the Complex-Domain spectral difference function taking into account changes in magnitude and phase. It emphasizes note onsets either as a result of significant change in energy in the magnitude spectrum, and/or a deviation from the expected phase values in the phase spectrum, caused by a change in pitch." } },
	{ axiom::tsn::weight_complexPhase,          RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the simplified Complex-Domain spectral difference function taking into account phase changes, weighted by magnitude. It reacts better on tonal sounds such as bowed string, but tends to over-detect percussive events."} },
	{ axiom::tsn::weight_flux,                  RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the Spectral Flux detection function which characterizes changes in magnitude spectrum." } },
	{ axiom::tsn::weight_rms,                   RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the difference function, measuring the half-rectified change of the RMS of the magnitude spectrum (i.e., measuring overall energy flux)" } },
    { axiom::tsn::weight_novelty,               RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
    "the novelty curve function reveals the predominant local pulse. works well with soft onsets." } },
    { axiom::tsn::refinementNumEventSubdivisions,           RangedSettingsSpec<int>{ { 1, 16 }, 1,
        "For each detected sound event, subdivide by this amount. Uses a special cumulative energy algorithm rather than equal subdivisions." } },
    { axiom::tsn::refinementSilenceThresholdDb,             RangedSettingsSpec<double>{ {-100.0, 0.0, 0.1}, -50.0,
        "The silence threshold for the post-onset detection silence detection algorithm, which is used only to create events from silences", 1, "dB" } },
    { axiom::tsn::refinementMinSilenceDurationMs,           RangedSettingsSpec<double>{ { 0.0, 5000.0, 1.0, 0.25 }, 300.0,
        "The minimum length for a silence to be counted as such", 0, "ms" } },
    { axiom::tsn::refinementMinEventWithinSilenceDurationMs, RangedSettingsSpec<double>{ { 0.0, 5000.0, 1.0, 0.25 }, 600.0,
"The minimum length for an event found within a silence to be counted as such", 0, "ms" } }
};

const std::map<juce::String, AnySpec> pitchSpecs
{
	{ axiom::tsn::pitchDetectionAlgorithm,  ChoiceSettingsSpec{ {axiom::tsn::yin, axiom::tsn::yinFFT, axiom::tsn::pYin,axiom::tsn::chroma}, axiom::tsn::yin } },

    { axiom::tsn::frameSize, RangedSettingsSpec<int>{ {64, 16384, 1, 1}, 4096, "Recommended: at least 4096 for reliable pitch detection" } },
    { axiom::tsn::hopSize,   RangedSettingsSpec<int>{ {32, 8192, 1, 1}, 2048, "" } },

    { axiom::tsn::interpolate,              BoolSettingsSpec{ true } },
	{ axiom::tsn::maxFrequency,             RangedSettingsSpec<double>{ {20.0,22050.0, 1.0, 1.0}, 4000.0 } },
	{ axiom::tsn::minFrequency,             RangedSettingsSpec<double>{ {20.0,22050.0, 1.0, 1.0},  140.0 } },
	{ axiom::tsn::tolerance,                RangedSettingsSpec<double>{ {0.0, 1.0,  0.001f, 1.0},   0.15 } },

    { axiom::tsn::replace_dismal_confidences_with_constant, BoolSettingsSpec{ true } },
    { axiom::tsn::dismal_confidence_threshold, RangedSettingsSpec<double>{ {0.0, 1.0, 0.0 }, 0.0, "The maximum pitch confidence at which the detected pitch value is allowed to pass without replacement.", 3 } },
    { axiom::tsn::dismal_replacement_constant, ChoiceSettingsSpec{ {"negative", "zero", "nyquist"}, "negative", "The value with which to replace any detected pitches with low confidence. "} },

    { axiom::tsn::lowRMSThreshold, RangedSettingsSpec<double>{{ 0.0, 1.0 }, 0.1, "", 2} },
    { axiom::tsn::preciseTime, BoolSettingsSpec{ false } }
};

const std::map<juce::String, AnySpec> loudnessSpecs
{
    {axiom::tsn::equalizeLoudness, BoolSettingsSpec{true}}
};

const std::map<juce::String, AnySpec> pitchSalienceSpecs
{
    { axiom::tsn::highBoundary, RangedSettingsSpec<double>{ {0.0, 22050.0, 1.0, 1.0}, 5000.0, "Upper frequency boundary for pitch salience analysis [Hz]", 1, "Hz" } },
    { axiom::tsn::lowBoundary,  RangedSettingsSpec<double>{ {0.0, 22050.0, 1.0, 1.0}, 100.0, "Lower frequency boundary for pitch salience analysis [Hz]", 1, "Hz" } }
};

const std::map<juce::String, AnySpec> splitSpecs
{
	{ axiom::tsn::fadeInSamps,  RangedSettingsSpec<int>{ {0,10000,1,1}, 5 } },
	{ axiom::tsn::fadeOutSamps, RangedSettingsSpec<int>{ {0,10000,1,1}, 5 } }
};

const std::map<juce::String, AnySpec> pacmapSpecs
{
    { axiom::tsn::num_neighbours,RangedSettingsSpec<int>{ {5, 35, 1, 1},  15, "Number of nearest neighbors." } },
    { axiom::tsn::MN_ratio,      RangedSettingsSpec<double>{ {0.1, 10.0}, 0.5, "Mid-near pairs (ratio to num nearest neighbors)." } },
    { axiom::tsn::FP_ratio,      RangedSettingsSpec<double>{ {0.1, 10.0}, 2.0, "Further points edges (ratio to num nearest neighbors). Increasing FP_ratio pulls global structure apart, which can break string-like collapse" } },
    { axiom::tsn::learning_rate, RangedSettingsSpec<double>{ {0.1, 10.0},  1, "Learning rate." } },
    { axiom::tsn::phase_1_iters, RangedSettingsSpec<int>{ {1, 200, 1, 1}, 100 } },
    { axiom::tsn::phase_2_iters, RangedSettingsSpec<int>{ {1, 200, 1, 1}, 100 } },
    { axiom::tsn::preprocess_mode, ChoiceSettingsSpec {{axiom::tsn::Normalize, axiom::tsn::Standardize}, axiom::tsn::Normalize,
        "Whether to use range-based normalization or z-score based standardization for initialization."}}
};

const std::map<juce::String, const std::map<juce::String,AnySpec>*>
	specsByBranch
{
	{ axiom::tsn::Analysis, &analysisSpecs },
	{ axiom::tsn::BFCC,     &bfccSpecs     },
	{ axiom::tsn::Onset,    &onsetSpecs    },
	{ axiom::tsn::Pitch,    &pitchSpecs    },
	{ axiom::tsn::Loudness, &loudnessSpecs },
	{ axiom::tsn::PitchSalience, &pitchSalienceSpecs },
	{ axiom::tsn::PaCMAP,   &pacmapSpecs   },
	{ axiom::tsn::Split,    &splitSpecs    },
};

void ensureBranchAndInitializeDefaults (ValueTree& settingsVT,
										const juce::String& branchName) {
	auto branchVT = settingsVT.getOrCreateChildWithName (branchName, nullptr);

	if (const auto it = specsByBranch.find (branchName); it != specsByBranch.end()) {
        for (auto const& specMap = *it->second; const auto&[fst, snd] : specMap) {
			auto propName = fst;
			auto const& branchSpec = snd;

			std::visit ([&]<typename T0>(T0&& spec){
				using SpecT = std::decay_t<T0>;

				if constexpr (std::is_same_v<SpecT, RangedSettingsSpec<int>> ||
							  std::is_same_v<SpecT, RangedSettingsSpec<double>>) { // NOLINT
					branchVT.setProperty (propName, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, ChoiceSettingsSpec>) {
					branchVT.setProperty (propName, spec.defaultValue, nullptr);
				}
				else if constexpr (std::is_same_v<SpecT, BoolSettingsSpec>) {
					branchVT.setProperty (propName, spec.defaultValue, nullptr);
				}
			}, branchSpec);
		}
	}
}

void initializeSettingsBranches(ValueTree& settingsVT, const bool dbg){
	for (auto& [branchName, _] : specsByBranch) {
		ensureBranchAndInitializeDefaults (settingsVT, branchName);
	}
    if constexpr (!TIMBRE_SPACE_SETTINGS_EXIST) {
        if (const auto timbreSpaceSettingsTree = settingsVT.getChildWithName(axiom::tsn::TimbreSpace); timbreSpaceSettingsTree.isValid())
        {
            settingsVT.removeChild(timbreSpaceSettingsTree, nullptr);
            std::cout << "removed previously existing TimbreSpace settings subtree\n";
        }
    }
	if (dbg){
		std::cout << "initializeSettingsBranches tree: " << settingsVT.toXmlString();
	}
}

bool verifySettingsStructure (const ValueTree& settingsVT)
{
	if (! settingsVT.isValid()){
	    Logger::writeToLog("Settings refers to invalid data; returning...");
		return false;
	}
	for (auto const& [branchName, specMapPtr] : specsByBranch) {
		auto branchId = juce::Identifier (branchName);
		auto branchVT = settingsVT.getChildWithName (branchId);
		
		// check branch validity
		if (! branchVT.isValid()) {
    	    Logger::writeToLog("Branch " + branchId.toString() + " refers to invalid data; returning...");
			return false; // missing entire branch
		}

		// check every parameter key inside that branch
		for (auto const& [propertyName, spec] : *specMapPtr) {
            if (juce::Identifier propertyId (propertyName);
                !branchVT.hasProperty(propertyId))
            {
        	    Logger::writeToLog("Branch has no property " + propertyId.toString() + "; returning...");
				return false;
			}
		}
	}

	return true;
}
bool verifySettingsStructureWithAttemptedFix (ValueTree& settingsVT)
{
    if (! settingsVT.isValid()){
        jassertfalse;
        return false;
    }
    for (auto const& [branchName, specMapPtr] : specsByBranch) {
        auto branchId = juce::Identifier (branchName);
        auto branchVT = settingsVT.getChildWithName (branchId);

        // check branch validity
        if (! branchVT.isValid()) {
            ensureBranchAndInitializeDefaults (settingsVT, branchName);
            branchVT = settingsVT.getChildWithName (branchId);
            jassert(branchVT.isValid());
        }

        // check every parameter key inside that branch
        for (auto const& [fst, spec] : *specMapPtr) {
            const auto propertyName = fst;  // copy because structured bindings aren't lambda-captured in C++20
            if (juce::Identifier propertyId (propertyName);
                !branchVT.hasProperty(propertyId))
            {
                const auto it = specsByBranch.find (branchName);
                if (it == specsByBranch.end()) {
                    jassertfalse;
                    return false;
                }
                const std::map<juce::String, AnySpec> branchSpecs = *it->second;
                auto thing = branchSpecs.find(propertyName);
                if (thing == branchSpecs.end()) {
                    jassertfalse;
                    return false;
                }
                auto propSpec = thing->second;
                std::visit ([&]<typename T0>(T0&& anySpec){
                    using SpecT = std::decay_t<T0>;
                    branchVT.setProperty (propertyName, anySpec.defaultValue, nullptr);
                }, propSpec);
            }
        }
    }
    return true;
}

juce::ValueTree createParentTreeFromSettings(const AnalyzerSettings &settings) {
    juce::ValueTree parent("Root");

    // Create FileInfo node
    juce::ValueTree fileInfoTree(axiom::tsn::FileInfo);
    setTreeProperty(fileInfoTree, axiom::tsn::sampleFilePath, settings.info.sampleFilePath);
    setTreeProperty(fileInfoTree, axiom::tsn::sampleRate, settings.analysis.sampleRate);
    parent.appendChild(fileInfoTree, nullptr);

    // Create Settings tree
    juce::ValueTree settingsTree(axiom::tsn::Settings);

    // Analysis node
    juce::ValueTree analysisNode(axiom::tsn::Analysis);
    setTreeProperty(analysisNode, axiom::tsn::frameSize, settings.analysis.frameSize);
    setTreeProperty(analysisNode, axiom::tsn::hopSize, settings.analysis.hopSize);
    setTreeProperty(analysisNode, axiom::tsn::windowingType, settings.analysis.windowingType);
    setTreeProperty(analysisNode, axiom::tsn::numThreads, settings.analysis.numThreads);
    settingsTree.appendChild(analysisNode, nullptr);

    // BFCC node
    juce::ValueTree bfccNode(axiom::tsn::BFCC);
    setTreeProperty(bfccNode, axiom::tsn::dctType, settings.bfcc.dctType);
    setTreeProperty(bfccNode, axiom::tsn::highFrequencyBound, settings.bfcc.highFrequencyBound);
    setTreeProperty(bfccNode, axiom::tsn::liftering, settings.bfcc.liftering);
    setTreeProperty(bfccNode, axiom::tsn::lowFrequencyBound, settings.bfcc.lowFrequencyBound);
    setTreeProperty(bfccNode, axiom::tsn::normalize, settings.bfcc.normalize);
    setTreeProperty(bfccNode, axiom::tsn::numBands, settings.bfcc.numBands);
    setTreeProperty(bfccNode, axiom::tsn::numCoefficients, settings.bfcc.numCoefficients);
    setTreeProperty(bfccNode, axiom::tsn::spectrumType, settings.bfcc.spectrumType);
    setTreeProperty(bfccNode, axiom::tsn::weightingType, settings.bfcc.weightingType);
    setTreeProperty(bfccNode, axiom::tsn::BFCC0_frameNormalizationFactor, settings.bfcc.BFCC0_frameNormalizationFactor);
    setTreeProperty(bfccNode, axiom::tsn::BFCC0_eventNormalize, settings.bfcc.BFCC0_eventNormalize);
    settingsTree.appendChild(bfccNode, nullptr);

    // Onset node
    juce::ValueTree onsetNode(axiom::tsn::Onset);
    setTreeProperty(onsetNode, axiom::tsn::segmentation,
                    settings.onset.segmentation == AnalyzerSettings::Onset::Segmentation::Uniform
                        ? axiom::tsn::Uniform
                        : axiom::tsn::Event);
    setTreeProperty(onsetNode, axiom::tsn::alpha, settings.onset.alpha);
    setTreeProperty(onsetNode, axiom::tsn::numFrames_shortOnsetFilter, settings.onset.numFrames_shortOnsetFilter);
    setTreeProperty(onsetNode, axiom::tsn::silenceThreshold, settings.onset.silenceThreshold);
    setTreeProperty(onsetNode, axiom::tsn::weight_complex, settings.onset.weight_complex);
    setTreeProperty(onsetNode, axiom::tsn::weight_complexPhase, settings.onset.weight_complexPhase);
    setTreeProperty(onsetNode, axiom::tsn::weight_flux, settings.onset.weight_flux);
    setTreeProperty(onsetNode, axiom::tsn::weight_hfc, settings.onset.weight_hfc);
    setTreeProperty(onsetNode, axiom::tsn::weight_rms, settings.onset.weight_rms);
    setTreeProperty(onsetNode, axiom::tsn::weight_novelty, settings.onset.weight_novelty);
    setTreeProperty(onsetNode, axiom::tsn::refinementNumEventSubdivisions,
                    settings.onset._refinement.numEventSubdivisions);
    setTreeProperty(onsetNode, axiom::tsn::refinementSilenceThresholdDb, settings.onset._refinement.silenceThresholdDb);
    setTreeProperty(onsetNode, axiom::tsn::refinementMinSilenceDurationMs,
                    settings.onset._refinement.minSilenceDurationMs);
    setTreeProperty(onsetNode, axiom::tsn::refinementMinEventWithinSilenceDurationMs,
                    settings.onset._refinement.minEventWithinSilenceDurationMs);
    settingsTree.appendChild(onsetNode, nullptr);

    // Pitch node
    {
        juce::ValueTree pitchNode(axiom::tsn::Pitch);
        setTreeProperty(pitchNode, axiom::tsn::pitchDetectionAlgorithm, settings.pitch.pitchDetectionAlgorithm);
        setTreeProperty(pitchNode, axiom::tsn::frameSize, settings.pitch.frameSize);
        setTreeProperty(pitchNode, axiom::tsn::hopSize, settings.pitch.hopSize);

        setTreeProperty(pitchNode, axiom::tsn::replace_dismal_confidences_with_constant,
                        settings.pitch.replace_dismal_confidences_with_constant);
        setTreeProperty(pitchNode, axiom::tsn::dismal_confidence_threshold, settings.pitch.dismal_confidence_threshold);
        setTreeProperty(pitchNode, axiom::tsn::dismal_replacement_constant,
                        settings.pitch.dismal_replacement_constant);
        {
            /// TODO: make these subtrees. will involve changing validation and thus spec structure, and retrieval from tree
            setTreeProperty(pitchNode, axiom::tsn::maxFrequency, settings.pitch._yin.maxFrequency);
            setTreeProperty(pitchNode, axiom::tsn::minFrequency, settings.pitch._yin.minFrequency);
            setTreeProperty(pitchNode, axiom::tsn::interpolate, settings.pitch._yin.interpolate);
            setTreeProperty(pitchNode, axiom::tsn::tolerance, settings.pitch._yin.tolerance);
        }
        {
            setTreeProperty(pitchNode, axiom::tsn::lowRMSThreshold, settings.pitch._pYin.lowRMSThreshold);
            setTreeProperty(pitchNode, axiom::tsn::preciseTime, settings.pitch._pYin.preciseTime);
        }
        settingsTree.appendChild(pitchNode, nullptr);
    }

    // Loudness node
    juce::ValueTree loudnessNode(axiom::tsn::Loudness);
    setTreeProperty(loudnessNode, axiom::tsn::equalizeLoudness, settings.loudness.equalizeLoudness);
    settingsTree.appendChild(loudnessNode, nullptr);

    // PitchSalience node
    juce::ValueTree pitchSalienceNode(axiom::tsn::PitchSalience);
    setTreeProperty(pitchSalienceNode, axiom::tsn::highBoundary, settings.pitchSalience.highBoundary);
    setTreeProperty(pitchSalienceNode, axiom::tsn::lowBoundary, settings.pitchSalience.lowBoundary);
    settingsTree.appendChild(pitchSalienceNode, nullptr);

    // Split node
    juce::ValueTree splitNode(axiom::tsn::Split);
    setTreeProperty(splitNode, axiom::tsn::fadeInSamps, settings.split.fadeInSamps);
    setTreeProperty(splitNode, axiom::tsn::fadeOutSamps, settings.split.fadeOutSamps);
    settingsTree.appendChild(splitNode, nullptr);

    juce::ValueTree pacmapNode{axiom::tsn::PaCMAP};
    setTreeProperty(pacmapNode, axiom::tsn::num_neighbours, settings.pacmap.num_neighbours);
    setTreeProperty(pacmapNode, axiom::tsn::MN_ratio, settings.pacmap.MN_ratio);
    setTreeProperty(pacmapNode, axiom::tsn::FP_ratio, settings.pacmap.FP_ratio);
    setTreeProperty(pacmapNode, axiom::tsn::learning_rate, settings.pacmap.learning_rate);
    setTreeProperty(pacmapNode, axiom::tsn::phase_1_iters, settings.pacmap.phase_1_iters);
    setTreeProperty(pacmapNode, axiom::tsn::phase_2_iters, settings.pacmap.phase_2_iters);
    setTreeProperty(pacmapNode, axiom::tsn::preprocess_mode, static_cast<int>(settings.pacmap.preprocess_mode));
    settingsTree.appendChild(pacmapNode, nullptr);

    // Add settings tree to parent
    parent.appendChild(settingsTree, nullptr);

    return parent;
}

bool updateSettingsFromValueTree(AnalyzerSettings& settings, const ValueTree& settingsTree) {
    auto parent = settingsTree.getParent();
    auto const fileInfoTree = parent.getChildWithName(axiom::tsn::FileInfo);
    settings.info.sampleFilePath = fileInfoTree.getProperty(axiom::tsn::sampleFilePath).toString();
    // settings.info.author = parent.getProperty("author").toString();
    if (!fileInfoTree.hasProperty(axiom::tsn::sampleRate)) {
        std::cerr << "Parent node missing required properties\n";
        jassertfalse;
        return false;
    }
    settings.analysis.sampleRate = fileInfoTree.getProperty(axiom::tsn::sampleRate);
    jassert(0.0 < settings.analysis.sampleRate);


    auto analysisNode = ValueTree{};
    if (!requireChildTree(settingsTree, axiom::tsn::Analysis, analysisNode) ||
        !validateProperties(analysisNode, {axiom::tsn::frameSize, axiom::tsn::hopSize, axiom::tsn::windowingType, axiom::tsn::numThreads}))
    {
        return false;
    }
    if (!loadRequiredProperties(analysisNode,
            prop(axiom::tsn::frameSize, settings.analysis.frameSize),
            prop(axiom::tsn::hopSize, settings.analysis.hopSize),
            prop(axiom::tsn::windowingType, settings.analysis.windowingType),
            prop(axiom::tsn::numThreads, settings.analysis.numThreads)))
    {
        return false;
    }

    // BFCC settings
    auto bfccNode = ValueTree{};
    if (!requireChildTree(settingsTree, axiom::tsn::BFCC, bfccNode) ||
        !validateProperties(bfccNode, {axiom::tsn::dctType, axiom::tsn::highFrequencyBound, axiom::tsn::liftering,
            axiom::tsn::lowFrequencyBound, axiom::tsn::normalize, axiom::tsn::numBands,
            axiom::tsn::numCoefficients, axiom::tsn::spectrumType, axiom::tsn::weightingType,
            axiom::tsn::BFCC0_frameNormalizationFactor, axiom::tsn::BFCC0_eventNormalize}))
    {
        return false;
    }
    if (!loadRequiredProperties(bfccNode,
            prop(axiom::tsn::dctType, settings.bfcc.dctType),
            prop(axiom::tsn::highFrequencyBound, settings.bfcc.highFrequencyBound),
            prop(axiom::tsn::liftering, settings.bfcc.liftering),
            prop(axiom::tsn::lowFrequencyBound, settings.bfcc.lowFrequencyBound),
            prop(axiom::tsn::normalize, settings.bfcc.normalize),
            prop(axiom::tsn::numBands, settings.bfcc.numBands),
            prop(axiom::tsn::numCoefficients, settings.bfcc.numCoefficients),
            prop(axiom::tsn::spectrumType, settings.bfcc.spectrumType),
            prop(axiom::tsn::weightingType, settings.bfcc.weightingType),
            prop(axiom::tsn::BFCC0_frameNormalizationFactor, settings.bfcc.BFCC0_frameNormalizationFactor),
            prop(axiom::tsn::BFCC0_eventNormalize, settings.bfcc.BFCC0_eventNormalize)))
    {
        return false;
    }

    // Onset settings
    auto onsetNode = ValueTree{};
    if (!requireChildTree(settingsTree, axiom::tsn::Onset, onsetNode) ||
        !validateProperties(onsetNode, {axiom::tsn::alpha, axiom::tsn::numFrames_shortOnsetFilter,
            axiom::tsn::silenceThreshold, axiom::tsn::segmentation,
            axiom::tsn::weight_complex, axiom::tsn::weight_complexPhase,
            axiom::tsn::weight_flux, axiom::tsn::weight_hfc, axiom::tsn::weight_rms}))
    {
        return false;
    }
    if (!loadRequiredProperties(onsetNode,
        prop(axiom::tsn::alpha, settings.onset.alpha),
        prop(axiom::tsn::numFrames_shortOnsetFilter, settings.onset.numFrames_shortOnsetFilter),
        prop(axiom::tsn::silenceThreshold, settings.onset.silenceThreshold),
        prop(axiom::tsn::weight_complex, settings.onset.weight_complex),
        prop(axiom::tsn::weight_complexPhase, settings.onset.weight_complexPhase),
        prop(axiom::tsn::weight_flux, settings.onset.weight_flux),
        prop(axiom::tsn::weight_hfc, settings.onset.weight_hfc),
        prop(axiom::tsn::weight_novelty, settings.onset.weight_novelty),
        prop(axiom::tsn::refinementNumEventSubdivisions, settings.onset._refinement.numEventSubdivisions),
        prop(axiom::tsn::refinementSilenceThresholdDb, settings.onset._refinement.silenceThresholdDb),
        prop(axiom::tsn::refinementMinSilenceDurationMs, settings.onset._refinement.minSilenceDurationMs),
        prop(axiom::tsn::refinementMinEventWithinSilenceDurationMs, settings.onset._refinement.minEventWithinSilenceDurationMs)))
    {
        return false;
    }
    // assigning to an enum member; loadRequiredProperties does not know how to convert juce::var to enum
    if (onsetNode.hasProperty(axiom::tsn::segmentation)) {
        const auto segmentationStr = onsetNode.getProperty(axiom::tsn::segmentation).toString();
        settings.onset.segmentation = segmentationStr == axiom::tsn::Uniform ? AnalyzerSettings::Onset::Segmentation::Uniform : AnalyzerSettings::Onset::Segmentation::Event;
    } else {
        settings.onset.segmentation = AnalyzerSettings::Onset::Segmentation::Event;
        DBG(juce::String("No property ") + axiom::tsn::segmentation + " found in settingsTree\n");
    }

    // Pitch settings
    {
        auto pitchNode = settingsTree.getChildWithName(axiom::tsn::Pitch);
        if (!pitchNode.isValid()) {
            std::cerr << "Pitch node missing\n";
            jassertfalse;
            return false;
        }
        if (!pitchNode.hasProperty(axiom::tsn::pitchDetectionAlgorithm)) {
            std::cerr << "Pitch node missing required properties\n";
            jassertfalse;
            return false;
        }

        settings.pitch.pitchDetectionAlgorithm = pitchNode.getProperty(axiom::tsn::pitchDetectionAlgorithm).toString();
        const auto pFrameSize = pitchNode.getProperty(axiom::tsn::frameSize);
        const auto pHopSize = pitchNode.getProperty(axiom::tsn::hopSize);

        if (const auto isNumeric = [](const var& v) { return v.isDouble() || v.isInt() || v.isInt64(); };
            isNumeric(pFrameSize) && isNumeric(pHopSize))
        {
            settings.pitch.frameSize = juce::nextPowerOfTwo(std::max(16, static_cast<int>(pFrameSize)));
            settings.pitch.hopSize = juce::nextPowerOfTwo(std::max(16, static_cast<int>(pHopSize)));
        }
        else {
            std::cerr << "Pitch node missing frameSize and/or hopSize; using defaults\n";
            pitchNode.setProperty(axiom::tsn::frameSize, settings.pitch.frameSize, nullptr);
            pitchNode.setProperty(axiom::tsn::hopSize, settings.pitch.hopSize, nullptr);
        }

        {
            if (!pitchNode.hasProperty(axiom::tsn::interpolate) || !pitchNode.hasProperty(axiom::tsn::maxFrequency) ||
                !pitchNode.hasProperty(axiom::tsn::minFrequency) || !pitchNode.hasProperty(axiom::tsn::tolerance))
            {
                std::cerr << "Pitch node missing required properties\n";
                jassertfalse;
                return false;
            }
            settings.pitch._yin.interpolate = pitchNode.getProperty(axiom::tsn::interpolate);
            settings.pitch._yin.maxFrequency = pitchNode.getProperty(axiom::tsn::maxFrequency);
            settings.pitch._yin.minFrequency = pitchNode.getProperty(axiom::tsn::minFrequency);
            settings.pitch._yin.tolerance = pitchNode.getProperty(axiom::tsn::tolerance);

            settings.pitch._pYin.lowRMSThreshold = pitchNode.getProperty(axiom::tsn::lowRMSThreshold);
            settings.pitch._pYin.preciseTime = pitchNode.getProperty(axiom::tsn::preciseTime);
        }
    }

    // Loudness settings
    auto loudnessNode = ValueTree{};
    if (!requireChildTree(settingsTree, axiom::tsn::Loudness, loudnessNode) ||
        !validateProperties(loudnessNode, {axiom::tsn::equalizeLoudness}))
    {
        return false;
    }
    if (!loadRequiredProperty(loudnessNode, axiom::tsn::equalizeLoudness, settings.loudness.equalizeLoudness))
    {
        return false;
    }

    // PitchSalience settings
    auto pitchSalienceNode = ValueTree{};
    if (!requireChildTree(settingsTree, axiom::tsn::PitchSalience, pitchSalienceNode) ||
        !validateProperties(pitchSalienceNode, {axiom::tsn::highBoundary, axiom::tsn::lowBoundary}))
    {
        return false;
    }
    if (!loadRequiredProperties(pitchSalienceNode,
        prop(axiom::tsn::highBoundary, settings.pitchSalience.highBoundary),
        prop(axiom::tsn::lowBoundary, settings.pitchSalience.lowBoundary)))
    {
        return false;
    }

	// Split settings
	auto splitNode = ValueTree{};
	if (!requireChildTree(settingsTree, axiom::tsn::Split, splitNode) ||
	    !validateProperties(splitNode, {axiom::tsn::fadeInSamps, axiom::tsn::fadeOutSamps}))
	{
		return false;
	}
	if (!loadRequiredProperties(splitNode,
	    prop(axiom::tsn::fadeInSamps, settings.split.fadeInSamps),
	    prop(axiom::tsn::fadeOutSamps, settings.split.fadeOutSamps)))
	{
		return false;
	}

    auto pacmapNode = ValueTree{};
    if (!requireChildTree(settingsTree, axiom::tsn::PaCMAP, pacmapNode)) {
        return false;
    }
    if (!loadRequiredProperties(pacmapNode,
        prop(axiom::tsn::num_neighbours, settings.pacmap.num_neighbours),
        prop(axiom::tsn::MN_ratio, settings.pacmap.MN_ratio),
        prop(axiom::tsn::FP_ratio, settings.pacmap.FP_ratio),
        prop(axiom::tsn::learning_rate, settings.pacmap.learning_rate),
        prop(axiom::tsn::phase_1_iters, settings.pacmap.phase_1_iters),
        prop(axiom::tsn::phase_2_iters, settings.pacmap.phase_2_iters)))
    {
        return false;
    }
    settings.pacmap.preprocess_mode =
        [&pacmapNode]() {
            const auto s = pacmapNode.getProperty(axiom::tsn::preprocess_mode).toString();
            if (s == axiom::tsn::Normalize) {
                return dim::PreprocessMode_e::Normalize;
            } if (s == axiom::tsn::Standardize) {
                return dim::PreprocessMode_e::Standardize;
            }
            jassertfalse;
            return dim::PreprocessMode_e::Normalize;
        }();
	return true;
}

}	// namespace nvs::analysis
