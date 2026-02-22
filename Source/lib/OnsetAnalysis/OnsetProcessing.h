//
// Created by Nicholas Solem on 10/12/25.
//

#pragma once
#include <vector>

namespace nvs::analysis {

void filterOnsets(std::vector<float> &onsetsInSeconds, const double lengthInSeconds, float minimumOnsetDeltaSeconds = 0.02f);

void improveOnsetsInSeconds(
    std::vector<float>&   onsetsInSeconds,
    const std::vector<float>&   wave,
    float                       sampleRate,
    float                       searchBackMs    = 500.0f,  // how far back to look for pre-onset silence
    float                       rmsWindowMs     = 30.0f    // RMS analysis window size
);

void forceMinimumOnsets(std::vector<float> &onsets, int minOnsets, double lengthInSeconds);

void equalizeOnsetDensity(std::vector<float> &onsets, double lengthInSeconds);

void normalizeOnsets(std::vector<float> &onsetsInSeconds, const double lengthInSeconds);

void denormalizeOnsets(std::vector<float> &normalizedOnsets, const double lengthInSeconds);

}

