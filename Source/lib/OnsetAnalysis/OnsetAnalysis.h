/*
  ==============================================================================

    OnsetAnalysis.h
    Created: 14 Jun 2023 10:28:35am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "AnalysisUsing.h"
#include "../Settings/Settings.h"
#include "../RunLoopStatus.h"

#include "essentia/utils/tnt/tnt2vector.h"
#include "essentia/essentiamath.h"

namespace nvs::analysis {

//===================================================================================
// e.g. vecReal const *sweepVec = new vecReal({ makeSweptSine(100.f, 1000.f, 5000) });
vecReal makeSweptSine(Real low, Real high, size_t len, Real sampleRate = 44100.f);
//===================================================================================

array2dReal calculateOnsetsMatrix(vecReal const &waveform, AnalyzerSettings const &settings,
                                  RunLoopStatus& rls, const ShouldExitFn &shouldExit);
vecReal calculateOnsetsInSeconds(const array2dReal &onsetAnalysisMatrix, AnalyzerSettings const &settings);

vecVecReal featuresForSbic(vecReal const &waveform, AnalyzerSettings const &settings,
                           RunLoopStatus& rls, const ShouldExitFn &shouldExit);
vecReal sBic(const array2dReal &featureMatrix, AnalyzerSettings const &settings);

vecVecReal splitWaveIntoEvents(vecReal const &wave, vecReal const &onsetsInSeconds, AnalyzerSettings const &settings,
                               RunLoopStatus& rls, const ShouldExitFn &shouldExit);


void writeWav(vecReal const &wave, std::string_view name, AnalyzerSettings const &settings,
              RunLoopStatus& rls, const ShouldExitFn &shouldExit);
void writeWavs(vecVecReal const &waves, std::string_view defName, AnalyzerSettings const &settings,
               RunLoopStatus& rls, const ShouldExitFn &shouldExit);

} // namespace nvs::analysis
