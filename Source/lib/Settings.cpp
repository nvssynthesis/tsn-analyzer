/*
  ==============================================================================

    Settings.cpp
    Created: 4 May 2025 2:13:31pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "Settings.h"
#include "Analyzer.h"
#include "StringAxiom.h"

namespace nvs::analysis {

static constexpr bool TIMBRE_SPACE_SETTINGS_EXIST {false};  // these 'settings' were meant to be automatable, so they are now parameters

using ValueTree = juce::ValueTree;
using NormalisableRangeDouble = juce::NormalisableRange<double>;

static NormalisableRangeDouble makePowerOfTwoRange (double minValue, double maxValue)
{
    auto minLog = std::log2 (minValue), maxLog = std::log2 (maxValue);
    return {
        minValue, maxValue,
        [=] (double, double, double n) { return std::pow (2.0, juce::jmap (n, 0.0, 1.0, minLog, maxLog)); },
        [=] (double, double, double v) { return juce::jmap (std::log2(v), minLog, maxLog, 0.0, 1.0); },
        [] (double s, double e, double v) { auto c=juce::jlimit(s, e, v); return std::pow(2.0,std::round(std::log2(double(c)))); }
    };
}

const std::map<juce::String, AnySpec> analysisSpecs
{
	{ axiom::tsn::frameSize,     RangedSettingsSpec<int>{   makePowerOfTwoRange(64, 8192), 1024 } },
	{ axiom::tsn::hopSize,       RangedSettingsSpec<int>{   makePowerOfTwoRange(32, 4096),  512 } },
	{ axiom::tsn::windowingType,  ChoiceSettingsSpec{ 	{axiom::tsn::hann, axiom::tsn::hamming, axiom::tsn::hannnsgcq,
		axiom::tsn::triangular, axiom::tsn::square, axiom::tsn::blackmanharris62, axiom::tsn::blackmanharris70,
		axiom::tsn::blackmanharris74, 	axiom::tsn::blackmanharris92}, /* default: */		axiom::tsn::hann 		} },
    { axiom::tsn::numThreads, RangedSettingsSpec<int>{NormalisableRangeDouble(1, juce::SystemStats::getNumCpus()), juce::SystemStats::getNumPhysicalCpus(),
        "The number of threads used for timbral analysis. Higher # of threads => faster analysis, but limited testing has been done for greater than 1 thread."}}
};

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
	    "use magnitude or power spectrum"} },
	{ axiom::tsn::weightingType,       ChoiceSettingsSpec{ {axiom::tsn::warping,  axiom::tsn::linear},axiom::tsn::warping,
	    "type of weighting function for determining triangle area"} },
	{ axiom::tsn::dctType,             ChoiceSettingsSpec{ {axiom::tsn::typeII,   axiom::tsn::typeIII},axiom::tsn::typeII } }
};

const std::map<juce::String, AnySpec> onsetSpecs
{
    { axiom::tsn::segmentation, ChoiceSettingsSpec {{axiom::tsn::Event, axiom::tsn::Uniform}, axiom::tsn::Event,
        "whether to segment by detected events or uniform frames"} },
	{ axiom::tsn::silenceThreshold,         RangedSettingsSpec<double>{ {0.0,1.0,0.01f,0.4}, 0.1f,
	    "the threshold for silence"} },
	{ axiom::tsn::alpha,                    RangedSettingsSpec<double>{ {0.0,1.0,0.01f,0.4}, 0.1f,
	    "the proportion of the mean included to reject smaller peaks; filters very short onsets" } },
	{ axiom::tsn::numFrames_shortOnsetFilter,RangedSettingsSpec<int>  { { 1,  64,  1,   1 },    5,
	    "the number of frames used to compute the threshold; size of short-onset filter"} },
	{ axiom::tsn::weight_hfc,               RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the High Frequency Content detection function which accurately detects percussive events" } },
	{ axiom::tsn::weight_complex,           RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.1,
	    "the Complex-Domain spectral difference function taking into account changes in magnitude and phase. It emphasizes note onsets either as a result of significant change in energy in the magnitude spectrum, and/or a deviation from the expected phase values in the phase spectrum, caused by a change in pitch." } },
	{ axiom::tsn::weight_complexPhase,      RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the simplified Complex-Domain spectral difference function taking into account phase changes, weighted by magnitude. It reacts better on tonal sounds such as bowed string, but tends to over-detect percussive events."} },
	{ axiom::tsn::weight_flux,              RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the Spectral Flux detection function which characterizes changes in magnitude spectrum." } },
	{ axiom::tsn::weight_rms,               RangedSettingsSpec<double>{ {0.0,1.0,0.01f,1.0},  0.0,
	    "the difference function, measuring the half-rectified change of the RMS of the magnitude spectrum (i.e., measuring overall energy flux)" } }
};

const std::map<juce::String, AnySpec> sBicSpecs
{
	{ axiom::tsn::complexityPenaltyWeight, RangedSettingsSpec<double>{ {0.0,10.0,0.1f,1.0}, 1.5f } },
	{ axiom::tsn::incrementFirstPass,      RangedSettingsSpec<int>{   {1,500,1,1},      60  } },
	{ axiom::tsn::incrementSecondPass,     RangedSettingsSpec<int>{   {1,500,1,1},      20  } },
	{ axiom::tsn::minSegmentLengthFrames,  RangedSettingsSpec<int>{   {1,100,1,1},      10  } },
	{ axiom::tsn::sizeFirstPass,           RangedSettingsSpec<int>{   {1,1000,1,1},     300 } },
	{ axiom::tsn::sizeSecondPass,          RangedSettingsSpec<int>{   {1,1000,1,1},     200 } }
};

const std::map<juce::String, AnySpec> pitchSpecs
{
	{ axiom::tsn::pitchDetectionAlgorithm,  ChoiceSettingsSpec{ {axiom::tsn::yin,axiom::tsn::pYin,axiom::tsn::chroma}, axiom::tsn::yin } },
	{ axiom::tsn::interpolate,              BoolSettingsSpec{ true } },
	{ axiom::tsn::maxFrequency,             RangedSettingsSpec<double>{ {20.0,22050.0, 1.0, 1.0}, 4000.0 } },
	{ axiom::tsn::minFrequency,             RangedSettingsSpec<double>{ {20.0,22050.0, 1.0, 1.0},  140.0 } },
	{ axiom::tsn::tolerance,                RangedSettingsSpec<double>{ {0.0, 1.0,  0.001f, 1.0},   0.15 } }
};

const std::map<juce::String, AnySpec> loudnessSpecs
{
    {axiom::tsn::equalizeLoudness, BoolSettingsSpec{true}}
};

const std::map<juce::String, AnySpec> splitSpecs
{
	{ axiom::tsn::fadeInSamps,  RangedSettingsSpec<int>{ {0,10000,1,1}, 5 } },
	{ axiom::tsn::fadeOutSamps, RangedSettingsSpec<int>{ {0,10000,1,1}, 5 } }
};

const std::map<juce::String, const std::map<juce::String,AnySpec>*>
	specsByBranch
{
	{ axiom::tsn::Analysis, &analysisSpecs },
	{ axiom::tsn::BFCC,     &bfccSpecs     },
	{ axiom::tsn::Onset,    &onsetSpecs    },
	{ axiom::tsn::sBic,     &sBicSpecs     },
	{ axiom::tsn::Pitch,    &pitchSpecs    },
	{ axiom::tsn::Loudness,  &loudnessSpecs    },
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
							  std::is_same_v<SpecT, RangedSettingsSpec<double>>) {
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
		return false;
	}
	for (auto const& [branchName, specMapPtr] : specsByBranch) {
		auto branchId = juce::Identifier (branchName);
		auto branchVT = settingsVT.getChildWithName (branchId);
		
		// check branch validity
		if (! branchVT.isValid()) {
			return false; // missing entire branch
		}

		// check every parameter key inside that branch
		for (auto const& [propertyName, spec] : *specMapPtr) {
            if (juce::Identifier propertyId (propertyName);
                !branchVT.hasProperty(propertyId))
            {
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

juce::ValueTree createParentTreeFromSettings(const AnalyzerSettings& settings) {
    juce::ValueTree parent("Root");

    // Create FileInfo node
    juce::ValueTree fileInfoTree(axiom::tsn::FileInfo);
    fileInfoTree.setProperty(axiom::tsn::sampleFilePath, settings.info.sampleFilePath, nullptr);
    fileInfoTree.setProperty(axiom::tsn::sampleRate, settings.analysis.sampleRate, nullptr);
    parent.appendChild(fileInfoTree, nullptr);

    // Create Settings tree
    juce::ValueTree settingsTree("Settings");

    // Analysis node
    juce::ValueTree analysisNode(axiom::tsn::Analysis);
    analysisNode.setProperty(axiom::tsn::frameSize, settings.analysis.frameSize, nullptr);
    analysisNode.setProperty(axiom::tsn::hopSize, settings.analysis.hopSize, nullptr);
    analysisNode.setProperty(axiom::tsn::windowingType, settings.analysis.windowingType, nullptr);
    analysisNode.setProperty(axiom::tsn::numThreads, settings.analysis.numThreads, nullptr);
    settingsTree.appendChild(analysisNode, nullptr);

    // BFCC node
    juce::ValueTree bfccNode(axiom::tsn::BFCC);
    bfccNode.setProperty(axiom::tsn::dctType, settings.bfcc.dctType, nullptr);
    bfccNode.setProperty(axiom::tsn::highFrequencyBound, settings.bfcc.highFrequencyBound, nullptr);
    bfccNode.setProperty(axiom::tsn::liftering, settings.bfcc.liftering, nullptr);
    bfccNode.setProperty(axiom::tsn::lowFrequencyBound, settings.bfcc.lowFrequencyBound, nullptr);
    bfccNode.setProperty(axiom::tsn::normalize, settings.bfcc.normalize, nullptr);
    bfccNode.setProperty(axiom::tsn::numBands, settings.bfcc.numBands, nullptr);
    bfccNode.setProperty(axiom::tsn::numCoefficients, settings.bfcc.numCoefficients, nullptr);
    bfccNode.setProperty(axiom::tsn::spectrumType, settings.bfcc.spectrumType, nullptr);
    bfccNode.setProperty(axiom::tsn::weightingType, settings.bfcc.weightingType, nullptr);
    bfccNode.setProperty(axiom::tsn::BFCC0_frameNormalizationFactor, settings.bfcc.BFCC0_frameNormalizationFactor, nullptr);
    bfccNode.setProperty(axiom::tsn::BFCC0_eventNormalize, settings.bfcc.BFCC0_eventNormalize, nullptr);
    settingsTree.appendChild(bfccNode, nullptr);

    // Onset node
    juce::ValueTree onsetNode(axiom::tsn::Onset);
    onsetNode.setProperty(axiom::tsn::segmentation,
        settings.onset.segmentation == AnalyzerSettings::Onset::Segmentation::Uniform ? axiom::tsn::Uniform : "Event", nullptr);
    onsetNode.setProperty(axiom::tsn::alpha, settings.onset.alpha, nullptr);
    onsetNode.setProperty(axiom::tsn::numFrames_shortOnsetFilter, settings.onset.numFrames_shortOnsetFilter, nullptr);
    onsetNode.setProperty(axiom::tsn::silenceThreshold, settings.onset.silenceThreshold, nullptr);
    onsetNode.setProperty(axiom::tsn::weight_complex, settings.onset.weight_complex, nullptr);
    onsetNode.setProperty(axiom::tsn::weight_complexPhase, settings.onset.weight_complexPhase, nullptr);
    onsetNode.setProperty(axiom::tsn::weight_flux, settings.onset.weight_flux, nullptr);
    onsetNode.setProperty(axiom::tsn::weight_hfc, settings.onset.weight_hfc, nullptr);
    onsetNode.setProperty(axiom::tsn::weight_rms, settings.onset.weight_rms, nullptr);
    settingsTree.appendChild(onsetNode, nullptr);

    // Pitch node
    juce::ValueTree pitchNode(axiom::tsn::Pitch);
    pitchNode.setProperty(axiom::tsn::interpolate, settings.pitch.interpolate, nullptr);
    pitchNode.setProperty(axiom::tsn::maxFrequency, settings.pitch.maxFrequency, nullptr);
    pitchNode.setProperty(axiom::tsn::minFrequency, settings.pitch.minFrequency, nullptr);
    pitchNode.setProperty(axiom::tsn::pitchDetectionAlgorithm, settings.pitch.pitchDetectionAlgorithm, nullptr);
    pitchNode.setProperty(axiom::tsn::tolerance, settings.pitch.tolerance, nullptr);
    settingsTree.appendChild(pitchNode, nullptr);

    // Loudness node
    juce::ValueTree loudnessNode(axiom::tsn::Loudness);
    loudnessNode.setProperty(axiom::tsn::equalizeLoudness, settings.loudness.equalizeLoudness, nullptr);
    settingsTree.appendChild(loudnessNode, nullptr);

    // Split node
    juce::ValueTree splitNode(axiom::tsn::Split);
    splitNode.setProperty(axiom::tsn::fadeInSamps, settings.split.fadeInSamps, nullptr);
    splitNode.setProperty(axiom::tsn::fadeOutSamps, settings.split.fadeOutSamps, nullptr);
    settingsTree.appendChild(splitNode, nullptr);

    // sBic node
    juce::ValueTree sBicNode(axiom::tsn::sBic);
    sBicNode.setProperty(axiom::tsn::complexityPenaltyWeight, settings.sBic.complexityPenaltyWeight, nullptr);
    sBicNode.setProperty(axiom::tsn::incrementFirstPass, settings.sBic.incrementFirstPass, nullptr);
    sBicNode.setProperty(axiom::tsn::incrementSecondPass, settings.sBic.incrementSecondPass, nullptr);
    sBicNode.setProperty(axiom::tsn::minSegmentLengthFrames, settings.sBic.minSegmentLengthFrames, nullptr);
    sBicNode.setProperty(axiom::tsn::sizeFirstPass, settings.sBic.sizeFirstPass, nullptr);
    sBicNode.setProperty(axiom::tsn::sizeSecondPass, settings.sBic.sizeSecondPass, nullptr);
    settingsTree.appendChild(sBicNode, nullptr);

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


    auto analysisNode = settingsTree.getChildWithName(axiom::tsn::Analysis);

    if (!analysisNode.isValid()) {
        std::cerr << "Analysis node missing\n";
        jassertfalse;
        return false;
    }
    if (!analysisNode.hasProperty(axiom::tsn::frameSize) || !analysisNode.hasProperty(axiom::tsn::hopSize)
        || !analysisNode.hasProperty(axiom::tsn::windowingType)) {
        std::cerr << "Analysis node missing required properties\n";
        jassertfalse;
        return false;
        }
    settings.analysis.frameSize = analysisNode.getProperty(axiom::tsn::frameSize);
    settings.analysis.hopSize = analysisNode.getProperty(axiom::tsn::hopSize);
    settings.analysis.windowingType = analysisNode.getProperty(axiom::tsn::windowingType).toString();
    settings.analysis.numThreads = analysisNode.getProperty(axiom::tsn::numThreads);

    // BFCC settings
    auto bfccNode = settingsTree.getChildWithName(axiom::tsn::BFCC);
    if (!bfccNode.isValid()) {
        std::cerr << "BFCC node missing\n";
        jassertfalse;
        return false;
    }
    if (!bfccNode.hasProperty(axiom::tsn::dctType) || !bfccNode.hasProperty(axiom::tsn::highFrequencyBound) ||
        !bfccNode.hasProperty(axiom::tsn::liftering) || !bfccNode.hasProperty(axiom::tsn::lowFrequencyBound) ||
        !bfccNode.hasProperty(axiom::tsn::normalize) || !bfccNode.hasProperty(axiom::tsn::numBands) ||
        !bfccNode.hasProperty(axiom::tsn::numCoefficients) || !bfccNode.hasProperty(axiom::tsn::spectrumType) ||
        !bfccNode.hasProperty(axiom::tsn::weightingType)
        || !bfccNode.hasProperty(axiom::tsn::BFCC0_frameNormalizationFactor) || !bfccNode.hasProperty(axiom::tsn::BFCC0_eventNormalize))
    {
        std::cerr << "BFCC node missing required properties\n";
        jassertfalse;
        return false;
    }
    settings.bfcc.dctType = bfccNode.getProperty(axiom::tsn::dctType).toString();
    settings.bfcc.highFrequencyBound = bfccNode.getProperty(axiom::tsn::highFrequencyBound);
    settings.bfcc.liftering = bfccNode.getProperty(axiom::tsn::liftering);
    settings.bfcc.lowFrequencyBound = bfccNode.getProperty(axiom::tsn::lowFrequencyBound);
    settings.bfcc.normalize = bfccNode.getProperty(axiom::tsn::normalize).toString();
    settings.bfcc.numBands = bfccNode.getProperty(axiom::tsn::numBands);
    settings.bfcc.numCoefficients = bfccNode.getProperty(axiom::tsn::numCoefficients);
    settings.bfcc.spectrumType = bfccNode.getProperty(axiom::tsn::spectrumType).toString();
    settings.bfcc.weightingType = bfccNode.getProperty(axiom::tsn::weightingType).toString();
    settings.bfcc.BFCC0_frameNormalizationFactor = bfccNode.getProperty(axiom::tsn::BFCC0_frameNormalizationFactor);
    settings.bfcc.BFCC0_eventNormalize = bfccNode.getProperty(axiom::tsn::BFCC0_eventNormalize);

    // Onset settings
    auto onsetNode = settingsTree.getChildWithName(axiom::tsn::Onset);
    if (!onsetNode.isValid()) {
        std::cerr << "Onset node missing\n";
        jassertfalse;
        return false;
    }
    if (!onsetNode.hasProperty(axiom::tsn::alpha) || !onsetNode.hasProperty(axiom::tsn::numFrames_shortOnsetFilter) ||
        !onsetNode.hasProperty(axiom::tsn::silenceThreshold) || !onsetNode.hasProperty(axiom::tsn::weight_complex) ||
        !onsetNode.hasProperty(axiom::tsn::weight_complexPhase) || !onsetNode.hasProperty(axiom::tsn::weight_flux) ||
        !onsetNode.hasProperty(axiom::tsn::weight_hfc) || !onsetNode.hasProperty(axiom::tsn::weight_rms))
    {
        std::cerr << "Onset node missing required properties\n";
        jassertfalse;
        return false;
    }
    if (onsetNode.hasProperty(axiom::tsn::segmentation)) {
        const auto segmentationStr = onsetNode.getProperty(axiom::tsn::segmentation).toString();
        settings.onset.segmentation = segmentationStr == axiom::tsn::Uniform ? AnalyzerSettings::Onset::Segmentation::Uniform : AnalyzerSettings::Onset::Segmentation::Event;
    } else {
        settings.onset.segmentation = AnalyzerSettings::Onset::Segmentation::Event;
        DBG(juce::String("No property ") + axiom::tsn::segmentation + " found in settingsTree\n");
    }
	settings.onset.alpha = onsetNode.getProperty(axiom::tsn::alpha);
	settings.onset.numFrames_shortOnsetFilter = onsetNode.getProperty(axiom::tsn::numFrames_shortOnsetFilter);
	settings.onset.silenceThreshold = onsetNode.getProperty(axiom::tsn::silenceThreshold);
	settings.onset.weight_complex = onsetNode.getProperty(axiom::tsn::weight_complex);
	settings.onset.weight_complexPhase = onsetNode.getProperty(axiom::tsn::weight_complexPhase);
	settings.onset.weight_flux = onsetNode.getProperty(axiom::tsn::weight_flux);
	settings.onset.weight_hfc = onsetNode.getProperty(axiom::tsn::weight_hfc);
	settings.onset.weight_rms = onsetNode.getProperty(axiom::tsn::weight_rms);
	
	// Pitch settings
	auto pitchNode = settingsTree.getChildWithName(axiom::tsn::Pitch);
	if (!pitchNode.isValid()) {
		std::cerr << "Pitch node missing\n";
		jassertfalse;
		return false;
	}
	if (!pitchNode.hasProperty(axiom::tsn::interpolate) || !pitchNode.hasProperty(axiom::tsn::maxFrequency) ||
		!pitchNode.hasProperty(axiom::tsn::minFrequency) || !pitchNode.hasProperty(axiom::tsn::pitchDetectionAlgorithm) ||
		!pitchNode.hasProperty(axiom::tsn::tolerance)) {
		std::cerr << "Pitch node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.pitch.interpolate = pitchNode.getProperty(axiom::tsn::interpolate);
	settings.pitch.maxFrequency = pitchNode.getProperty(axiom::tsn::maxFrequency);
	settings.pitch.minFrequency = pitchNode.getProperty(axiom::tsn::minFrequency);
	settings.pitch.pitchDetectionAlgorithm = pitchNode.getProperty(axiom::tsn::pitchDetectionAlgorithm).toString();
	settings.pitch.tolerance = pitchNode.getProperty(axiom::tsn::tolerance);

    // Loudness settings
    auto loudnessNode = settingsTree.getChildWithName(axiom::tsn::Loudness);
    if (!loudnessNode.isValid()) {
        std::cerr << "Loudness node missing\n";
        jassertfalse;
        return false;
    }
    if (!loudnessNode.hasProperty(axiom::tsn::equalizeLoudness)) {
        std::cerr << "Loudness node missing required properties\n";
        jassertfalse;
        return false;
    }
    settings.loudness.equalizeLoudness = loudnessNode.getProperty(axiom::tsn::equalizeLoudness);

	// Split settings
	auto splitNode = settingsTree.getChildWithName(axiom::tsn::Split);
	if (!splitNode.isValid()) {
		std::cerr << "Split node missing\n";
		jassertfalse;
		return false;
	}
	if (!splitNode.hasProperty(axiom::tsn::fadeInSamps) || !splitNode.hasProperty(axiom::tsn::fadeOutSamps)) {
		std::cerr << "Split node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.split.fadeInSamps = splitNode.getProperty(axiom::tsn::fadeInSamps);
	settings.split.fadeOutSamps = splitNode.getProperty(axiom::tsn::fadeOutSamps);
	
	// TimbreSpace settings
    if constexpr (TIMBRE_SPACE_SETTINGS_EXIST) {
        ValueTree timbreSpaceNode = settingsTree.getChildWithName("TimbreSpace");
        if (!timbreSpaceNode.isValid()) {
            std::cerr << "TimbreSpace node missing\n";
            jassertfalse;
            return false;
        }
        if (!timbreSpaceNode.hasProperty("HistogramEqualization") || !timbreSpaceNode.hasProperty("xAxis") ||
            !timbreSpaceNode.hasProperty("yAxis")) {
            std::cerr << "TimbreSpace node missing required properties\n";
            jassertfalse;
            return false;
            }
    }

	// sBic settings
	auto sBicNode = settingsTree.getChildWithName(axiom::tsn::sBic);
	if (!sBicNode.isValid()) {
		std::cerr << "sBic node missing\n";
		jassertfalse;
		return false;
	}
	if (!sBicNode.hasProperty(axiom::tsn::complexityPenaltyWeight) || !sBicNode.hasProperty(axiom::tsn::incrementFirstPass) ||
		!sBicNode.hasProperty(axiom::tsn::incrementSecondPass) || !sBicNode.hasProperty(axiom::tsn::minSegmentLengthFrames) ||
		!sBicNode.hasProperty(axiom::tsn::sizeFirstPass) || !sBicNode.hasProperty(axiom::tsn::sizeSecondPass)) {
		std::cerr << "sBic node missing required properties\n";
		jassertfalse;
		return false;
	}
	settings.sBic.complexityPenaltyWeight = sBicNode.getProperty(axiom::tsn::complexityPenaltyWeight);
	settings.sBic.incrementFirstPass = sBicNode.getProperty(axiom::tsn::incrementFirstPass);
	settings.sBic.incrementSecondPass = sBicNode.getProperty(axiom::tsn::incrementSecondPass);
	settings.sBic.minSegmentLengthFrames = sBicNode.getProperty(axiom::tsn::minSegmentLengthFrames);
	settings.sBic.sizeFirstPass = sBicNode.getProperty(axiom::tsn::sizeFirstPass);
	settings.sBic.sizeSecondPass = sBicNode.getProperty(axiom::tsn::sizeSecondPass);
	
	return true;
}

}	// namespace nvs::analysis
