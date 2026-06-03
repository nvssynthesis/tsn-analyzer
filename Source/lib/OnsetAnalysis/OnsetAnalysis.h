/*
  ==============================================================================

    OnsetAnalysis.h
    Created: 14 Jun 2023 10:28:35am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include "AnalysisUsing.h"
#include "../RunLoopStatus.h"
#include "Settings/ModernSettingsTypes.h"

#include "essentia/essentiamath.h"

namespace nvs::analysis {

//===================================================================================
// e.g. vecReal const *sweepVec = new vecReal({ makeSweptSine(100.f, 1000.f, 5000) });
vecReal makeSweptSine(Real low, Real high, size_t len, Real sampleRate = 44100.f);
//===================================================================================

array2dReal calculateOnsetsMatrix(const vecReal &waveform, double sampleRate,
            const modern::AnalyzerSettingsRegistry &settings,
            RunLoopStatus& rls, const ShouldExitFn &shouldExit);
vecReal calculateOnsetsInSeconds(const array2dReal &onsetAnalysisMatrix, const modern::AnalyzerSettingsRegistry &settings);

vecVecReal splitWaveIntoEvents(const vecReal &wave,
                            double sampleRate,
                            const vecReal &onsetsInSeconds,
                            const modern::AnalyzerSettingsRegistry &settings,
                            RunLoopStatus& rls, const ShouldExitFn &shouldExit);


void writeWav(const vecReal &wave, double sampleRate, std::string_view name, const modern::AnalyzerSettingsRegistry &settings,
              RunLoopStatus& rls, const ShouldExitFn &shouldExit);
void writeWavs(const vecVecReal &waves, double sampleRate, std::string_view defName, const modern::AnalyzerSettingsRegistry &settings,
               RunLoopStatus& rls, const ShouldExitFn &shouldExit);

} // namespace nvs::analysis
