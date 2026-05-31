//
// Created by Nicholas Solem on 1/28/26.
//

#pragma once

namespace nvs::axiom::tsn {
#ifndef STRAXIOMIZE
#define STRAXIOMIZE(x) inline constexpr char x[] {#x}
#endif

STRAXIOMIZE(Metadata);
STRAXIOMIZE(super);
STRAXIOMIZE(Version);

STRAXIOMIZE(CreationTime);
STRAXIOMIZE(sampleFilePath);
STRAXIOMIZE(sampleRate);
STRAXIOMIZE(audioHash);
STRAXIOMIZE(FileInfo);
STRAXIOMIZE(settingsHash);
STRAXIOMIZE(AnalysisSettings);
STRAXIOMIZE(Analysis);
STRAXIOMIZE(Settings);
STRAXIOMIZE(analysisFile);

STRAXIOMIZE(TimbreAnalysis);
STRAXIOMIZE(NormalizedOnsets);
STRAXIOMIZE(TimbreMeasurements);

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

STRAXIOMIZE(BFCC);
STRAXIOMIZE(BFCC0);
STRAXIOMIZE(BFCC1);
STRAXIOMIZE(BFCC2);
STRAXIOMIZE(BFCC3);
STRAXIOMIZE(BFCC4);
STRAXIOMIZE(BFCC5);
STRAXIOMIZE(BFCC6);
STRAXIOMIZE(BFCC7);
STRAXIOMIZE(BFCC8);
STRAXIOMIZE(BFCC9);
STRAXIOMIZE(BFCC10);
STRAXIOMIZE(BFCC11);
STRAXIOMIZE(BFCC12);
STRAXIOMIZE(spectral);
STRAXIOMIZE(pitch);
STRAXIOMIZE(loudness);
STRAXIOMIZE(SpectralCentroid);
STRAXIOMIZE(SpectralDecrease);
STRAXIOMIZE(SpectralFlatness);
STRAXIOMIZE(SpectralCrest);
STRAXIOMIZE(SpectralComplexity);
STRAXIOMIZE(StrongPeak);
STRAXIOMIZE(PitchSalience);
STRAXIOMIZE(highBoundary);
STRAXIOMIZE(lowBoundary);

//
STRAXIOMIZE(magnitudeThreshold);
STRAXIOMIZE(SpectralPeak);
STRAXIOMIZE(magnitudeThreshold_dB);
STRAXIOMIZE(maxPeaks);

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

STRAXIOMIZE(equalizeLoudness);

STRAXIOMIZE(Onset);
STRAXIOMIZE(segmentation);
STRAXIOMIZE(doRefinements);
STRAXIOMIZE(refinementNumEventSubdivisions);
STRAXIOMIZE(refinementSilenceThresholdDb);
STRAXIOMIZE(refinementMinSilenceDurationMs);
STRAXIOMIZE(refinementMinEventWithinSilenceDurationMs);
STRAXIOMIZE(Event);
STRAXIOMIZE(Uniform);
STRAXIOMIZE(uniformEventLength);
STRAXIOMIZE(alpha);
STRAXIOMIZE(numFrames_shortOnsetFilter);
STRAXIOMIZE(silenceThreshold);
STRAXIOMIZE(weight_complex);
STRAXIOMIZE(weight_complexPhase);
STRAXIOMIZE(weight_flux);
STRAXIOMIZE(weight_hfc);
STRAXIOMIZE(weight_rms);
STRAXIOMIZE(weight_novelty);
STRAXIOMIZE(Pitch);
STRAXIOMIZE(pitchDetectionAlgorithm);
STRAXIOMIZE(yin);
STRAXIOMIZE(pYin);
STRAXIOMIZE(yinFFT);
STRAXIOMIZE(chroma);
// pitch confidences
STRAXIOMIZE(replace_dismal_confidences_with_constant);
STRAXIOMIZE(dismal_confidence_threshold);
STRAXIOMIZE(dismal_replacement_constant);
// yin
STRAXIOMIZE(interpolate);
STRAXIOMIZE(maxFrequency);
STRAXIOMIZE(minFrequency);
STRAXIOMIZE(tolerance);
// pyin
STRAXIOMIZE(lowRMSThreshold);
STRAXIOMIZE(preciseTime);

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

STRAXIOMIZE(TimbreSpace);

STRAXIOMIZE(DRMode);
STRAXIOMIZE(x_axis);
STRAXIOMIZE(y_axis);
STRAXIOMIZE(z_axis);
STRAXIOMIZE(w_axis);
STRAXIOMIZE(u_axis);
STRAXIOMIZE(v_axis);
STRAXIOMIZE(nav_tendency_x);
STRAXIOMIZE(nav_tendency_y);
STRAXIOMIZE(histogram_equalization);
STRAXIOMIZE(filtered_feature);
STRAXIOMIZE(filtered_feature_min);
STRAXIOMIZE(filtered_feature_max);
STRAXIOMIZE(decorrelateFromPitchAndLoudness);

STRAXIOMIZE(statistic);
STRAXIOMIZE(mean);
STRAXIOMIZE(median);
STRAXIOMIZE(variance);
STRAXIOMIZE(skewness);
STRAXIOMIZE(kurtosis);

STRAXIOMIZE(Frame);
STRAXIOMIZE(BFCCs);
STRAXIOMIZE(Periodicity);
STRAXIOMIZE(Loudness);
STRAXIOMIZE(f0);

STRAXIOMIZE(PaCMAP);
STRAXIOMIZE(PaCMAP0);
STRAXIOMIZE(PaCMAP1);
STRAXIOMIZE(phase_1_iters);
STRAXIOMIZE(phase_2_iters);
STRAXIOMIZE(num_neighbours);
STRAXIOMIZE(learning_rate);
STRAXIOMIZE(MN_ratio);
STRAXIOMIZE(FP_ratio);
// preprocessing options
STRAXIOMIZE(preprocess_mode);
STRAXIOMIZE(Normalize);
STRAXIOMIZE(Standardize);

STRAXIOMIZE(saveAnalysis);
STRAXIOMIZE(onsetsAvailable);
STRAXIOMIZE(shapedPointsAvailable);
STRAXIOMIZE(timbreSpaceTreeChanged);

STRAXIOMIZE(tsn_granular);
STRAXIOMIZE(Analyses);

#undef STRAXIOMIZE
}