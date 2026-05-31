//
// Created by Nicholas Solem on 3/3/26.
//

#pragma once
#include "AnalysisUsing.h"
#include "../Settings/Settings.h"
#include <span>

#include "Settings/ModernSettingsTypes.h"

namespace nvs::analysis {


struct PitchesAndConfidences {
    std::vector<Real> pitches, confidences;
};

PitchesAndConfidences calculatePitchesAndConfidences(
    const vecReal &waveEvent,
    double sampleRate,
    modern::AnalyzerSettingsRegistry const& settings);


}
