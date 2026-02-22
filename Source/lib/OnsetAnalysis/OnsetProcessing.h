//
// Created by Nicholas Solem on 10/12/25.
//

#pragma once
#include <vector>

namespace nvs::analysis {

void filterOnsets(std::vector<float> &onsetsInSeconds, double lengthInSeconds, float minimumOnsetDeltaSeconds = 0.02f);

#pragma message("When relevant, take in the precomputed RMS envelope instead of the raw waveform.")

void improveOnsetsInSeconds(
    std::vector<float>&   onsetsInSeconds,
    const std::vector<float>&   wave,
    float                       sampleRate,
    float                       searchBackMs    = 500.0f,  // how far back to look for pre-onset silence
    float                       rmsWindowMs     = 30.0f    // RMS analysis window size
);
void subdivideOnsetsEnergy(std::vector<float>& onsetsInSeconds, const std::vector<float>& wave,
    float sampleRate, unsigned int numSubsections,
    float rmsHopProportion = 0.005f,
    float minimumSubdivisionLengthMs = 500.f);

void subdivideOnsetsNaive(std::vector<float>& onsetsInSeconds, const std::vector<float>& wave, float sampleRate, unsigned int numSubsections);

void forceMinimumOnsets(std::vector<float> &onsets, int minOnsets, double lengthInSeconds);

void equalizeOnsetDensity(std::vector<float> &onsets, double lengthInSeconds);

void normalizeOnsets(std::vector<float> &onsetsInSeconds, double lengthInSeconds);

void denormalizeOnsets(std::vector<float> &normalizedOnsets, double lengthInSeconds);

}

