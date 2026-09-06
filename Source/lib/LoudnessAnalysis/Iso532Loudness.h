/*
  ==============================================================================

    Iso532Loudness.h

    Wraps the vendored ISO 532-1 (Zwicker) loudness reference implementation. Its
    header (Source/ISO 532-1 - Program etc/...) CANNOT BE included from here.
    Only Iso532Loudness.cpp touches it, IF AND ONLY IF TSN_HAVE_ISO532 is defined.
    This gets set by CMake. Without it, calculateIso532Loudness always returns
    std::nullopt, ensuring that callers remain agnostic to inclusion of the vendored code.

  ==============================================================================
*/

#pragma once

#include "AnalysisUsing.h"
#include <array>
#include <optional>
#include <vector>

namespace nvs::analysis {

// ISO 532-1's N_BARK_BANDS: specific loudness resolution, 0.1 Bark per band.
inline constexpr int NumSpecificLoudnessBands = 240;

struct Iso532LoudnessSeries {
    vecReal loudnessSone;
    std::vector<std::array<float, NumSpecificLoudnessBands>> specificLoudness;
};

// returns std::nullopt if the excerpt is too short to produce any output,
// OR if the vendored library wasn't available at build time.
std::optional<Iso532LoudnessSeries> calculateIso532Loudness(
    const vecReal &waveEvent, double sampleRate, bool diffuseField);

}   // namespace nvs::analysis
