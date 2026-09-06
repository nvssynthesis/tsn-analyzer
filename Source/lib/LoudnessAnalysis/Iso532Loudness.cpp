/*
  ==============================================================================

    Iso532Loudness.cpp

  ==============================================================================
*/

#include "Iso532Loudness.h"

namespace nvs::analysis {

#ifdef TSN_HAVE_ISO532

#include "ISO_532-1.h"

static_assert(N_BARK_BANDS == NumSpecificLoudnessBands,
    "ISO 532-1 header's band count changed; update NumSpecificLoudnessBands to match");

namespace {

vecReal resampleTo48k(const vecReal &wave, const float sourceSampleRate) {
    constexpr int bestQuality = 0;
    const auto resampler = std::unique_ptr<standard::Algorithm>(StandardFactory::create(
        "Resample",
        "inputSampleRate", sourceSampleRate,
        "outputSampleRate", 48000.f,
        "quality", bestQuality));
    vecReal resampled;
    resampler->input("signal").set(wave);
    resampler->output("signal").set(resampled);
    resampler->compute();
    return resampled;
}

} // anonymous namespace

std::optional<Iso532LoudnessSeries> calculateIso532Loudness(
    const vecReal &waveEvent, const double sampleRate, const bool diffuseField)
{
    const vecReal wave48k = (sampleRate == 48000.0)
        ? waveEvent
        : resampleTo48k(waveEvent, static_cast<float>(sampleRate));
    if (wave48k.empty()) return std::nullopt;

    // matches f_loudness_from_signal's own internal sizing formula for TimeVarying mode,
    // so our output buffers are exactly right-sized (SizeOutput must be >= this).
    constexpr int decFactorLevel = static_cast<int>(48000.0 / SR_LEVEL);
    const int numSamplesLevel = static_cast<int>(wave48k.size()) / decFactorLevel;
    if (numSamplesLevel < 1) return std::nullopt;

    std::vector<double> waveDouble(wave48k.begin(), wave48k.end());

    InputData signal {
        .NumSamples = static_cast<int>(waveDouble.size()),
        .SampleRate = 48000.0,
        .pData = waveDouble.data()
    };

    std::vector<double> outLoudness(static_cast<size_t>(numSamplesLevel));
    std::vector<std::vector<double>> specLoudnessRows(
        N_BARK_BANDS, std::vector<double>(static_cast<size_t>(numSamplesLevel)));
    std::array<double*, N_BARK_BANDS> specLoudnessPtrs {};
    for (int band = 0; band < N_BARK_BANDS; ++band) {
        specLoudnessPtrs[static_cast<size_t>(band)] = specLoudnessRows[static_cast<size_t>(band)].data();
    }

    const int numWritten = f_loudness_from_signal(
        &signal,
        diffuseField ? SoundFieldDiffuse : SoundFieldFree,
        LoudnessMethodTimeVarying,
        0.0,   // TimeSkip: unused by LoudnessMethodTimeVarying (only Stationary mode reads it)
        outLoudness.data(),
        specLoudnessPtrs.data(),
        numSamplesLevel);

    if (numWritten <= 0) return std::nullopt;

    Iso532LoudnessSeries result;
    result.loudnessSone.reserve(static_cast<size_t>(numWritten));
    result.specificLoudness.reserve(static_cast<size_t>(numWritten));
    for (int t = 0; t < numWritten; ++t) {
        result.loudnessSone.push_back(static_cast<float>(outLoudness[static_cast<size_t>(t)]));

        std::array<float, NumSpecificLoudnessBands> bands {};
        for (int band = 0; band < N_BARK_BANDS; ++band) {
            bands[static_cast<size_t>(band)] =
                static_cast<float>(specLoudnessRows[static_cast<size_t>(band)][static_cast<size_t>(t)]);
        }
        result.specificLoudness.push_back(bands);
    }
    return result;
}

#else // !TSN_HAVE_ISO532

std::optional<Iso532LoudnessSeries> calculateIso532Loudness(
    const vecReal &/*waveEvent*/, double /*sampleRate*/, bool /*diffuseField*/)
{
    return std::nullopt;
}

#endif

} // namespace nvs::analysis
