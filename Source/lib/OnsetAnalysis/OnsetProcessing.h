//
// Created by Nicholas Solem on 10/12/25.
//

#pragma once
#include <span>
#include <vector>

namespace nvs::analysis {

void filterOnsetsOutsideBounds(std::vector<float> &onsetsInSeconds, double lengthInSeconds, float minimumProximityToEndAllowedSeconds = 0.02f);
void filterRedundantOnsets(std::vector<float> &onsetsInSeconds, float minimumOnsetDeltaSeconds = 0.02f);

#pragma message("When relevant, take in the precomputed RMS envelope instead of the raw waveform.")

void improveOnsetsInSeconds(
    std::vector<float>&         onsetsInSeconds,
    std::span<const float>      wave,
    float                       sampleRate,
    float                       searchBackMs    = 500.0f,  // how far back to look for pre-onset silence
    float                       rmsWindowMs     = 30.0f,    // RMS analysis window size
    bool                        giveChanceBeforeStart = true // enables algo that lets first onset potentially move to sample 0 if it otherwise would be unchanged
);

void subdivideOnsetsNaive(std::vector<float>& onsetsInSeconds, std::span<const float>  wave, float sampleRate, unsigned int numSubsections);

void subdivideOnsetsEnergy(std::vector<float>& onsetsInSeconds, std::span<const float>  wave, float sampleRate,
    unsigned int numSubsections,
    float rmsHopProportion = 0.002f,
    float minimumSubdivisionLengthMs = 100.f);

void forceMinimumOnsets(std::vector<float> &onsets, int minOnsets, double lengthInSeconds);

void equalizeOnsetDensity(std::vector<float> &onsets, double lengthInSeconds);

void normalizeOnsets(std::vector<float> &onsetsInSeconds, double lengthInSeconds);

void denormalizeOnsets(std::vector<float> &normalizedOnsets, double lengthInSeconds);


struct SilenceTimings {
    std::vector<float> silenceOnsets;
    std::vector<float> silenceOffsets;
};
SilenceTimings detectSilences(std::span<const float>  wave, float sampleRate,
    float silenceThresholdDb = -50.0f,
    float minSilenceDurationMs = 300.0f,
    float minEventDurationMs = 500.0f,
    float rmsHopMs = 5.f,
    float rmsAveragingWindowLength = 50.f);

void combineOnsetsAndSilenceTimings(std::vector<float>& onsetsInSeconds, const SilenceTimings &silenceTimings,
    float minDeltaOnsetToSilenceOnsetSeconds, float minDeltaSilenceOnsetToOnsetSeconds,
    float minDeltaOnsetToSilenceOffsetSeconds, float minDeltaSilenceOffsetToOnsetSeconds);

}

