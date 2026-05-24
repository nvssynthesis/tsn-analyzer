/*
  ==============================================================================

    TimbreAnalysis.h
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once

#include "AnalysisUsing.h"
#include "../Settings/Settings.h"
#include "../Features.h"
#include <span>

namespace nvs::analysis {

vecReal calculateLoudnesses(std::span<Real const> waveSpan, AnalyzerSettings const& settings);

FeatureContainer<vecReal> calculateTimbres(std::span<Real const> waveSpan, AnalyzerSettings const& settings);

vecVecReal PCA(vecVecReal const &V, int num_features_out);

} // namespace nvs::analysis
