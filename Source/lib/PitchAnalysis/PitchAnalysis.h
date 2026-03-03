//
// Created by Nicholas Solem on 3/3/26.
//

#pragma once
#include "AnalysisUsing.h"
#include "../Settings.h"
#include <span>

namespace nvs::analysis {


struct PitchesAndConfidences {
    std::vector<Real> pitches, confidences;
};

PitchesAndConfidences calculatePitchesAndConfidences(vecReal waveEvent, AnalyzerSettings const& settings);


}