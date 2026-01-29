//
// Created by Nicholas Solem on 1/28/26.
//

#pragma once

namespace nvs::axiom {
#ifndef STRAXIOMIZE
#define STRAXIOMIZE(x) inline constexpr const char* x {#x}
#endif

// STRAXIOMIZE(Metadata);
// STRAXIOMIZE(Version);
STRAXIOMIZE(FileInfo);
// STRAXIOMIZE(CreationTime);
STRAXIOMIZE(sampleFilePath);
STRAXIOMIZE(sampleRate);
STRAXIOMIZE(audioHash);
// STRAXIOMIZE(settingsHash);
// STRAXIOMIZE(AnalysisSettings);
STRAXIOMIZE(Analysis);
STRAXIOMIZE(Settings);
// STRAXIOMIZE(analysisFile);
//
// STRAXIOMIZE(TimbreAnalysis);
// STRAXIOMIZE(NormalizedOnsets);
// STRAXIOMIZE(TimbreMeasurements);
//
STRAXIOMIZE(numThreads);
STRAXIOMIZE(frameSize);
STRAXIOMIZE(hopSize);
STRAXIOMIZE(windowingType);
STRAXIOMIZE(hann);
STRAXIOMIZE(hamming);
STRAXIOMIZE(hannnsgcq);
STRAXIOMIZE(triangular);
STRAXIOMIZE(square);
STRAXIOMIZE(blackmanharris62);
STRAXIOMIZE(blackmanharris70);
STRAXIOMIZE(blackmanharris74);
STRAXIOMIZE(blackmanharris92);
//
STRAXIOMIZE(BFCC);
STRAXIOMIZE(SpectralCentroid);
STRAXIOMIZE(SpectralDecrease);
STRAXIOMIZE(SpectralFlatness);
STRAXIOMIZE(SpectralCrest);
STRAXIOMIZE(SpectralComplexity);
STRAXIOMIZE(StrongPeak);
//
STRAXIOMIZE(highFrequencyBound);
STRAXIOMIZE(liftering);
STRAXIOMIZE(lowFrequencyBound);
STRAXIOMIZE(normalize);
STRAXIOMIZE(BFCC0_frameNormalizationFactor);
STRAXIOMIZE(BFCC0_eventNormalize);
STRAXIOMIZE(unit_sum);
STRAXIOMIZE(unit_max);
STRAXIOMIZE(numBands);
STRAXIOMIZE(numCoefficients);
STRAXIOMIZE(spectrumType);
STRAXIOMIZE(magnitude);
STRAXIOMIZE(power);
STRAXIOMIZE(weightingType);
STRAXIOMIZE(warping);
STRAXIOMIZE(linear);
STRAXIOMIZE(dctType);
STRAXIOMIZE(typeII);
STRAXIOMIZE(typeIII);
//
STRAXIOMIZE(Onset);
STRAXIOMIZE(segmentation);
STRAXIOMIZE(Event);
STRAXIOMIZE(Uniform);
STRAXIOMIZE(alpha);
STRAXIOMIZE(numFrames_shortOnsetFilter);
STRAXIOMIZE(silenceThreshold);
STRAXIOMIZE(weight_complex);
STRAXIOMIZE(weight_complexPhase);
STRAXIOMIZE(weight_flux);
STRAXIOMIZE(weight_hfc);
STRAXIOMIZE(weight_rms);
STRAXIOMIZE(Pitch);
STRAXIOMIZE(equalizeLoudness);
STRAXIOMIZE(yin);
STRAXIOMIZE(pYin);
STRAXIOMIZE(chroma);
STRAXIOMIZE(interpolate);
STRAXIOMIZE(maxFrequency);
STRAXIOMIZE(minFrequency);
STRAXIOMIZE(pitchDetectionAlgorithm);
STRAXIOMIZE(tolerance);
STRAXIOMIZE(Split);
STRAXIOMIZE(fadeInSamps);
STRAXIOMIZE(fadeOutSamps);
STRAXIOMIZE(sBic);
STRAXIOMIZE(complexityPenaltyWeight);
STRAXIOMIZE(incrementFirstPass);
STRAXIOMIZE(incrementSecondPass);
STRAXIOMIZE(minSegmentLengthFrames);
STRAXIOMIZE(sizeFirstPass);
STRAXIOMIZE(sizeSecondPass);
//
STRAXIOMIZE(TimbreSpace);
//
// STRAXIOMIZE(x_axis);
// STRAXIOMIZE(y_axis);
// STRAXIOMIZE(z_axis);
// STRAXIOMIZE(w_axis);
// STRAXIOMIZE(u_axis);
// STRAXIOMIZE(v_axis);
// STRAXIOMIZE(nav_tendency_x);
// STRAXIOMIZE(nav_tendency_y);
// STRAXIOMIZE(histogram_equalization);
// STRAXIOMIZE(filtered_feature);
// STRAXIOMIZE(filtered_feature_min);
// STRAXIOMIZE(filtered_feature_max);
//
// STRAXIOMIZE(statistic);
// STRAXIOMIZE(mean);
// STRAXIOMIZE(median);
// STRAXIOMIZE(variance);
// STRAXIOMIZE(skewness);
// STRAXIOMIZE(kurtosis);
//
STRAXIOMIZE(Frame);
STRAXIOMIZE(BFCCs);
STRAXIOMIZE(Periodicity);
STRAXIOMIZE(Loudness);
STRAXIOMIZE(f0);
//
// inline const juce::String AudioFilePathAbsolute = "AudioFilePath (absolute)";
//
// STRAXIOMIZE(saveAnalysis);
// STRAXIOMIZE(onsetsAvailable);
// STRAXIOMIZE(shapedPointsAvailable);
// STRAXIOMIZE(timbreSpaceTreeChanged);
//
// STRAXIOMIZE(tsn_granular);
// STRAXIOMIZE(Analyses);

#undef STRAXIOMIZE
}