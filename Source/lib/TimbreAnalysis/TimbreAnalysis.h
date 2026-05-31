/*
  ==============================================================================

    TimbreAnalysis.h
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include "AnalysisUsing.h"
#include "../Features.h"
#include "Settings/ModernSettingsTypes.h"

namespace nvs::analysis {

vecReal calculateLoudnesses(const vecReal &waveform, modern::AnalyzerSettingsRegistry const& settings, double sampleRate);

FeatureContainer<vecReal> calculateTimbres(const vecReal &waveform, const modern::AnalyzerSettingsRegistry &settings, double sampleRate);

vecVecReal PCA(const vecVecReal &V, int num_features_out);

} // namespace nvs::analysis
