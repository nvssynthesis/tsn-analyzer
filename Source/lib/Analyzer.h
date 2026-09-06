/*
  ==============================================================================

    Analyzer.h
    Created: 12 Sep 2023 10:25:24pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <optional>

#include <juce_core/juce_core.h>

#include "RunLoopStatus.h"
#include "AnalysisUsing.h"
#include "Features.h"
#include "Statistics.h"
#include "PitchAnalysis/PitchAnalysis.h"
#include "Settings/ModernSettingsTypes.h"
#include "LoudnessAnalysis/Iso532Loudness.h"

namespace nvs::analysis {

template<typename T>
using FeatureContainerMemberPtr = T FeatureContainer<T>::*;

template<typename T>
[[nodiscard]]
std::vector<T>
extractFeatures(const FeatureContainer<T> &allFeatures,
				const std::vector<Feature_e> &featuresToUse)
{
	std::vector<T> v;
	v.reserve(featuresToUse.size());

	// one lookup table for the scalars
	auto const & scalarTable = allFeatures.features;

	for (auto f : featuresToUse) {
		v.push_back(scalarTable[static_cast<size_t>(f)]);
	}
	return v;
}

[[nodiscard]]
inline vecReal
extractFeatures(FeatureContainer<EventwiseStatistics<Real>> const &allFeatures,
				const std::vector<Feature_e> &featuresToUse,
				const Statistic statisticToUse)
{
	const auto descriptions = extractFeatures(allFeatures, featuresToUse);
	Real EventwiseStatistics<Real>::* ptr = nullptr;
	switch (statisticToUse) {
		case Statistic::Mean:     ptr = &EventwiseStatistics<Real>::mean;    	break;
		case Statistic::Median:   ptr = &EventwiseStatistics<Real>::median;  	break;
		case Statistic::Variance: ptr = &EventwiseStatistics<Real>::variance;	break;
		case Statistic::Skewness: ptr = &EventwiseStatistics<Real>::skewness;	break;
		case Statistic::Kurtosis: ptr = &EventwiseStatistics<Real>::kurtosis;	break;
	    case Statistic::NumStatistics: jassertfalse; break;
		default: jassertfalse;
	}

	std::vector<Real> out;
	out.reserve(descriptions.size());
	for (auto const & d : descriptions) {
		out.push_back(d.*ptr);
	}
	return out;
}

class Analyzer {
	ess::EssentiaInitializer ess_init;	  // this MUST be initialized before EssentiaHolder.
public:
	Analyzer();
	using EventwiseStats = EventwiseStatistics<Real>;

	std::optional<vecReal>
    calculateOnsetsInSeconds(
        const vecReal &wave,
        double sampleRate,
        RunLoopStatus& rls,
	    const ShouldExitFn &shouldExit) const;

	PitchesAndConfidences calculateEventwisePitchDescription(const vecReal &waveEvent, double sampleRate,
	    FeatureContainer<EventwiseStats> &features) const;  // now returns (raw) pitches/confidences, as the latter are needed for noisiness aggregate (it cannot just use the eventwise stats)
	void calculateEventwiseTimbreDescription(const vecReal &waveEvent, double sampleRate,
	    const PitchesAndConfidences& pitchesAndConfidences, FeatureContainer<EventwiseStats> &features) const;
	void calculateEventwiseLoudness(const vecReal &waveEvent, double sampleRate,
	    FeatureContainer<EventwiseStats> &features) const;
	// ISO 532-1 (Zwicker) loudness: a separate, psychoacoustically-accurate loudness measure
	// alongside calculateEventwiseLoudness's Essentia-based one (Feature_e::Loudness). Reduces the
	// onset's overall-loudness time series to the standard 5 EventwiseStats, and its specific-loudness
	// (per-critical-band) time series to a single mean vector -- see LoudnessAnalysis/Iso532Loudness.h.
	// Also derives the ACBFCC ("Auditory-Complete BFCC") cepstral coefficients from that same
	// specific-loudness time series (DCT down to 13 coefficients per internal frame, each reduced to
	// the standard 5 EventwiseStats -- see Feature_e::acbfcc0..acbfcc12), since both outputs are cheap
	// byproducts of the one calculateIso532Loudness call this function already makes.
	// A no-op (leaves all outputs at their default zero state) if the excerpt was too short or the
	// vendored ISO 532-1 library wasn't available at build time.
	void calculateEventwiseZwickerLoudness(const vecReal &waveEvent, double sampleRate,
	    FeatureContainer<EventwiseStats> &features,
	    std::array<float, NumSpecificLoudnessBands> &specificLoudnessOut) const;

	std::optional<std::vector<FeatureContainer<EventwiseStats>>>
    calculateOnsetwiseTimbreSpace(
        const vecReal &wave,
        double sampleRate,
        const vecReal &onsetsInSeconds,
        RunLoopStatus& rls,
        const ShouldExitFn &shouldExit,
        std::vector<std::array<float, NumSpecificLoudnessBands>> &specificLoudnessOut) const;

    std::optional<vecVecReal>
    calculatePaCMAP(const std::vector<FeatureContainer<EventwiseStats>> &timbreMeasurements) const;

    static std::optional<vecVecReal> calculatePCA(
	    const std::vector<FeatureContainer<EventwiseStats>> &allFeatures,
	    const std::vector<Feature_e> &featuresToUse,
	    Statistic statToUse);

    // returns null tree if no update was necessary
	[[nodiscard]] juce::ValueTree updateSettings(const juce::ValueTree &newSettings);
	modern::AnalyzerSettingsRegistry const &getSettings() const;
    ValueTree getSettingsParentTree() const;
    juce::String getSettingsHash() const;
    //====================================================================================
	ess::EssentiaHolder ess_hold;
private:
    modern::AnalyzerSettingsRegistry settings;
};

double getLengthInSeconds(auto lengthInSamples, auto sampleRate){
    jassert(sampleRate > 0);
	return static_cast<double>(lengthInSamples / sampleRate);
}

vecVecReal truncate(vecVecReal const &V, size_t trunc);
vecVecReal transpose(vecVecReal const &V);
vecVecReal transpose(std::span<const vecReal> V);

template <typename Func>
concept StatisticVectorFunction = requires(Func f, vecReal const &v) {
	{ f(v) } -> std::convertible_to<Real>;
};

template <StatisticVectorFunction Func>
vecReal binwiseStatistic(vecVecReal const &V, Func statisticFunc) {
	vecVecReal Vtranspose = transpose(V);
	size_t const sz = Vtranspose.size();
	vecReal results(sz);
	for (size_t i = 0; i < sz; ++i) {
		results[i] = statisticFunc(Vtranspose[i]);
	}
	return results;
}

void writeEventsToWav(
    const vecReal &wave,
    double sampleRate,
    const vecReal &onsetsInSeconds, std::string_view ogPath,
    const modern::AnalyzerSettingsRegistry &settings,
    RunLoopStatus& rls, const ShouldExitFn &shouldExit);

}	// namespace nvs::analysis
