//
// Created by Nicholas Solem on 3/3/26.
//

#pragma once
#include <span>

#include "AnalysisUsing.h"
#include "Settings/ModernSettingsTypes.h"

namespace nvs::analysis {


struct PitchesAndConfidences {
    vecReal pitches, confidences;
};

PitchesAndConfidences calculatePitchesAndConfidences(
    const vecReal &waveEvent,
    double sampleRate,
    const modern::AnalyzerSettingsRegistry &settings);


}
