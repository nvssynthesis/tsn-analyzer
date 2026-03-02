/*
  ==============================================================================

    TimbreAnalysis.cpp
    Created: 30 Oct 2023 2:07:11pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "TimbreAnalysis.h"

#include "StringAxiom.h"
#include "essentia/streaming/algorithms/poolstorage.h"

namespace nvs::analysis {

namespace {

PitchesAndConfidences processFrequenciesAndConfidences(vecReal &&frequencies, const vecReal &confidences, const AnalyzerSettings &settings){
    // convert frequency to pitch
    vecReal pitches = std::move(frequencies);
    assert(pitches.size() == confidences.size());

    // if we move to c++23, replace with zip iteration
    std::transform(pitches.begin(), pitches.end(), // first1, last1
        confidences.begin(),    // first2
        pitches.begin(),    // output
        [&settings](const float pitch, const float confidence) {
            if ((pitch <= 0.f) || (pitch >= settings.analysis.sampleRate * 0.5)) {
                return -100.0f;
            }
            if (settings.pitch.replace_dismal_confidences_with_constant) {
                if (confidence <= settings.pitch.dismal_confidence_threshold) {
                    return settings.pitch.dismal_replacement_constant;
                }
            }
            return 69.f + 12.f * std::log2(pitch / 440.f);
        }
    );

    return PitchesAndConfidences{
        .pitches = pitches,
        .confidences = confidences
    };
}

PitchesAndConfidences calculatePitchesEssentiaYin(std::span<Real> waveSpan, AnalyzerSettings const& settings){
    const vecReal wave(waveSpan.begin(), waveSpan.end());

    constexpr int frameSize = 4096;
    constexpr int zeroPadding = 2048;

    const auto frameCutter = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("FrameCutter",
                "frameSize",            frameSize,
                "hopSize",              settings.analysis.hopSize,
                "lastFrameToEndOfFile", true,
                "startFromZero",        true,
                "validFrameThresholdRatio", 0.f
            ));


    const auto windowing = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Windowing",
                "normalized", false,
                "size",        frameSize,
                "zeroPadding", zeroPadding,
                "type",        settings.analysis.windowingType.toStdString(),
                "zeroPhase",   false
            ));


    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYin",
                "sampleRate",   settings.analysis.sampleRate,
                "frameSize",   frameSize,
                "interpolate",  settings.pitch._yin.interpolate,
                "maxFrequency", settings.pitch._yin.maxFrequency,
                "minFrequency", settings.pitch._yin.minFrequency,
                "tolerance",    settings.pitch._yin.tolerance
            ));

    vecReal frequencies, confidences; // accumulate results manually
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(wave);
        frameCutter->output("frame").set(frame);
        frameCutter->compute();

        // check if done
        if (frame.empty()) break;

        // apply windowing
        vecReal windowedFrame;
        windowing->input("frame").set(frame);
        windowing->output("frame").set(windowedFrame);
        windowing->compute();

        // detect pitch
        Real pitch, pitchConfidence;
        pitchDet->input("signal").set(windowedFrame);
        pitchDet->output("pitch").set(pitch);
        pitchDet->output("pitchConfidence").set(pitchConfidence);
        pitchDet->compute();

        // accumulate results
        frequencies.push_back(pitch);
        confidences.push_back(pitchConfidence);
    }
    return processFrequenciesAndConfidences(std::move(frequencies), confidences, settings);
}

PitchesAndConfidences calculatePitchesEssentiaYinFFT(std::span<Real> waveSpan, AnalyzerSettings const& settings){
    const vecReal wave(waveSpan.begin(), waveSpan.end());

    constexpr int frameSize = 4096;
    constexpr int zeroPadding = 2048;

    const auto frameCutter = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("FrameCutter",
                "frameSize",            frameSize,
                "hopSize",              settings.analysis.hopSize,
                "lastFrameToEndOfFile", true,
                "startFromZero",        true,
                "validFrameThresholdRatio", 0.f
            ));


    const auto windowing = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Windowing",
                "normalized", false,
                "size",        frameSize,
                "zeroPadding", 0, //zeroPadding,
                "type",        "hann",
                "zeroPhase",   false
            ));

    const auto spectrum = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Spectrum",
                "size",  frameSize
                ));

    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYinFFT",
                "sampleRate",   settings.analysis.sampleRate,
                "frameSize",   frameSize,
                "interpolate",  settings.pitch._yin.interpolate,
                "maxFrequency", settings.pitch._yin.maxFrequency,
                "minFrequency", settings.pitch._yin.minFrequency,
                "tolerance",    settings.pitch._yin.tolerance,
                "weighting", "custom"   // {custom, A, B, C, D, Z}
            ));

    vecReal frequencies, confidences; // accumulate results manually
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(wave);
        frameCutter->output("frame").set(frame);
        frameCutter->compute();

        // check if done
        if (frame.empty()) break;

        // apply windowing
        vecReal windowedFrame;
        windowing->input("frame").set(frame);
        windowing->output("frame").set(windowedFrame);
        windowing->compute();

        // compute fft
        vecReal spec;
        spectrum->input("frame").set(windowedFrame);
        spectrum->output("spectrum").set(spec);
        spectrum->compute();

        // detect pitch
        Real pitch, pitchConfidence;
        pitchDet->input("spectrum").set(spec);
        pitchDet->output("pitch").set(pitch);
        pitchDet->output("pitchConfidence").set(pitchConfidence);
        pitchDet->compute();

        // accumulate results
        frequencies.push_back(pitch);
        confidences.push_back(pitchConfidence);
    }

    return processFrequenciesAndConfidences(std::move(frequencies), confidences, settings);
}

PitchesAndConfidences calculatePitchesEssentiaProbabilisticYin(std::span<Real> waveSpan, AnalyzerSettings const& settings){
    const vecReal wave(waveSpan.begin(), waveSpan.end());

    const auto outputUnvoiced = [&settings]() -> std::string {
        if (settings.pitch.replace_dismal_confidences_with_constant) {
            //  {zero, abs, negative}
            std::string retval =
                settings.pitch.dismal_replacement_constant < 0 ? "negative"
                : settings.pitch.dismal_replacement_constant == 0 ? "zero"
                    : "abs";
            return retval;
        }
        return "abs";
    }();

    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYinProbabilistic",
                "sampleRate",   settings.analysis.sampleRate,
                "frameSize",     settings.analysis.frameSize,
                "hopSize",      settings.analysis.hopSize,
                "lowRMSThreshold", settings.pitch._pYin.lowRMSThreshold,
                "preciseTime", settings.pitch._pYin.preciseTime,
                "outputUnvoiced", outputUnvoiced
            ));

    vecReal frequencies, confidences;

    pitchDet->input("signal").set(wave);
    pitchDet->output("pitch").set(frequencies);
    pitchDet->output("voicedProbabilities").set(confidences);

    pitchDet->compute();

    assert(frequencies.size() == confidences.size());
    {
        // convert frequency to pitch
        vecReal pitches = std::move(frequencies);
        assert(pitches.size() == confidences.size());

        // if we move to c++23, replace with zip iteration
        std::transform(pitches.begin(), pitches.end(), // first1, last1
            confidences.begin(),    // first2
            pitches.begin(),    // output
            [&pitchSettings = settings.pitch](const float pitch, const float confidence) {
                if (pitch <= 0.f) {
                    return 0.0f;
                }
        // NOTE: PitchYinProbabilistic already takes care of this EXCEPT the nyquist setting
                if (pitchSettings.replace_dismal_confidences_with_constant) {
                    if (confidence <= pitchSettings.dismal_confidence_threshold) {
                        return pitchSettings.dismal_replacement_constant;
                    }
                }
                const auto retval = 69.f + 12.f * std::log2(pitch / 440.f);
                if (std::isnan(retval)) {
                    jassertfalse;
                    return 0.0f;
                }
                return retval;
            }
        );
        return PitchesAndConfidences{pitches, confidences};
    }
}
}	// anonymous namespace

PitchesAndConfidences calculatePitchesAndConfidences (vecReal waveEvent,
                                                      AnalyzerSettings const& settings)
{
    auto const algo      = settings.pitch.pitchDetectionAlgorithm.toStdString();
    try {
        if (algo == axiom::tsn::yin) {
            return calculatePitchesEssentiaYin (waveEvent, settings);
        }
        if (algo == axiom::tsn::yinFFT) {
            return calculatePitchesEssentiaYinFFT(waveEvent, settings);
        }
        if (algo == axiom::tsn::pYin) {
            return calculatePitchesEssentiaProbabilisticYin (waveEvent, settings);
        }
        if (algo == axiom::tsn::chroma) {
            jassertfalse;  // not implemented
            return {};
            //		return calculatePitchesEssentiaChroma (waveEvent, factory, settingsTree);
        }
    }
    catch (const EssentiaException &e) {
        std::cerr << "EssentiaException: " << e.what() << std::endl;
        jassertfalse;
        return {};
    }
    jassertfalse;  // unknown algorithm
    return {};
}

vecReal calculateLoudnesses(const std::span<Real const> waveSpan, AnalyzerSettings const& settings)
{
    auto const filteredWave = [&settings](std::span<Real const> _waveSpan) -> vecReal
    {
        const vecReal wave(_waveSpan.begin(), _waveSpan.end());
        if (!settings.loudness.equalizeLoudness) {
            return wave;
        }

        [[maybe_unused]] const float originalSampleRate = settings.analysis.sampleRate;

        try {
            constexpr int rs_quality = 2;/* quality: SRC_SINC_FASTEST
                                 from enum {
                                           SRC_SINC_BEST_QUALITY       = 0,
                                           SRC_SINC_MEDIUM_QUALITY     = 1,
                                           SRC_SINC_FASTEST            = 2,
                                           SRC_ZERO_ORDER_HOLD         = 3,
                                           SRC_LINEAR                  = 4
                                       } ;
                                 */
            constexpr std::array permittedRates { 8000.f, 16000.f, 32000.f, 44100.f, 48000.f };
            auto resample = [](const vecReal &wave, const float source_sr, const float target_sr, const int resample_quality) -> vecReal {
                // resample for essentia's loudness algorithm
                const auto resampler	= std::unique_ptr<standard::Algorithm>(StandardFactory::create(
                    "Resample",
                 "inputSampleRate", source_sr,
                 "outputSampleRate", target_sr,
                 "quality",	resample_quality));
                vecReal resampledWave;
                resampler->input("signal").set(wave);
                resampler->output("signal").set(resampledWave);
                resampler->compute();
                return resampledWave;
            };

            const bool needsResample = std::ranges::find(permittedRates, originalSampleRate) == permittedRates.end();
            const float internal_sr = needsResample ? 44100.f : originalSampleRate;
            const auto internalWave = needsResample ?
                resample(wave, originalSampleRate, internal_sr, rs_quality) : wave ;
            const auto equalLoudnessFilter = std::unique_ptr<standard::Algorithm>(StandardFactory::create(
                    "EqualLoudness",
                    "sampleRate", internal_sr
                    ));

            // apply equal loudness filter to entire wave first
            vecReal w;
            equalLoudnessFilter->input("signal").set(internalWave);
            equalLoudnessFilter->output("signal").set(w);
            equalLoudnessFilter->compute();
            if (internal_sr == originalSampleRate) {
                return w;
            }
            jassert (internal_sr != originalSampleRate);
            // because otherwise we have to adjust hopSize & frameSize according to resampled rate
            if (needsResample) { w = resample(w, internal_sr, originalSampleRate, rs_quality); }
            return w;
        } catch (const essentia::EssentiaException &e) {
            std::cerr << e.what() << std::endl;
            jassertfalse;
            return {};
        }
    }(waveSpan);

    const int frameSize = settings.analysis.frameSize;
    const int hopSize = settings.analysis.hopSize;
    const auto frameCutter = std::unique_ptr<standard::Algorithm>(StandardFactory::create (
            "FrameCutter",
            "frameSize",               frameSize,
            "hopSize",                 hopSize,
            "lastFrameToEndOfFile",    true,
            "startFromZero",           true,
            "validFrameThresholdRatio", 0.0
            ));

    const auto windowing = std::unique_ptr<standard::Algorithm>(StandardFactory::create (
            "Windowing",
            "normalized",  false,
            "size",        frameSize,
            "zeroPadding", frameSize,
            "type",        settings.analysis.windowingType.toStdString(),
            "zeroPhase",   false
            ));

    const auto loudness = std::unique_ptr<standard::Algorithm>(StandardFactory::create("Loudness"));

    vecReal loudnesses; // NOLINT

    // Process frame by frame
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(filteredWave);
        frameCutter->output("frame").set(frame);
        frameCutter->compute();

        // check if done
        if (frame.empty()) break;

        // apply windowing
        vecReal windowedFrame;
        windowing->input("frame").set(frame);
        windowing->output("frame").set(windowedFrame);
        windowing->compute();

        // calculate loudness
        Real loudnessValue;
        loudness->input("signal").set(windowedFrame);
        loudness->output("loudness").set(loudnessValue);
        loudness->compute();

        // accumulate result
        loudnesses.push_back(loudnessValue);
    }

    return loudnesses;
}

FeatureContainer<vecReal> calculateTimbres(std::span<Real const> waveSpan, AnalyzerSettings const& settings)
{
    vecReal wave(waveSpan.begin(), waveSpan.end());

    const int frameSize  = settings.analysis.frameSize;

    for (auto &e : wave) {
        const float normFactor = 1.f;   // settings.analysis.frameSize;
        e *= normFactor;
    }

    const int hopSize = settings.analysis.hopSize;
    const auto frameCutter = std::unique_ptr<standard::Algorithm>(StandardFactory::create (
        "FrameCutter",
          "frameSize",               frameSize,
          "hopSize",                 hopSize,
          "lastFrameToEndOfFile",    true,
          "startFromZero",           true,
          "validFrameThresholdRatio", 0.0
    ));
    const auto windowing = std::unique_ptr<standard::Algorithm>(StandardFactory::create (
        "Windowing",
          "normalized",  false,
          "size",        frameSize,
          "zeroPadding", frameSize, // why am i even zero padding?
          "type",        settings.analysis.windowingType.toStdString(),
          "zeroPhase",   false
    ));
    auto const spectrumTypeStr = settings.bfcc.spectrumType.toStdString();
    bool const isPower = (spectrumTypeStr == "power");
    std::string const specAlgoStr = isPower ? "PowerSpectrum" : "Spectrum";
    const auto spectrum = std::unique_ptr<standard::Algorithm>(StandardFactory::create (
            specAlgoStr,
            "size", frameSize * 2
            ));
    std::map<juce::String, int> dctTypeStringToInt {
            { "typeII",  2 },
            { "typeIII", 3 }
    };
    std::map<std::string, std::string> logTypeMap {
            {"PowerSpectrum", "dbpow"},
            {"Spectrum", "dbamp"}
    };
    const auto sampleRate  = static_cast<float>(settings.analysis.sampleRate);
    const auto bfcc = std::unique_ptr<standard::Algorithm>(StandardFactory::create (
    "BFCC",
    "dctType",             dctTypeStringToInt.at(settings.bfcc.dctType.toStdString()),
    "highFrequencyBound",  settings.bfcc.highFrequencyBound,
    "inputSize",           frameSize + 1,
    "liftering",           settings.bfcc.liftering,
    "logType",             logTypeMap.at(specAlgoStr),

    "lowFrequencyBound",   settings.bfcc.lowFrequencyBound,
    "normalize",           settings.bfcc.normalize.toStdString(),
    "numberBands",         settings.bfcc.numBands,
    "numberCoefficients",  settings.bfcc.numCoefficients,
    "sampleRate",          sampleRate,
    "type",                spectrumTypeStr,
    "weighting",           settings.bfcc.weightingType.toStdString()
    ));
    const auto centroid_a = std::unique_ptr<standard::Algorithm>(StandardFactory::create ("Centroid",
        "range", sampleRate * 0.5));
    const auto decrease_a = std::unique_ptr<standard::Algorithm>(StandardFactory::create ("Decrease",
        "range", sampleRate * 0.5));
    const auto flatnessDB_a = std::unique_ptr<standard::Algorithm>(StandardFactory::create ("FlatnessDB"));
    const auto crest_a = std::unique_ptr<standard::Algorithm>(StandardFactory::create ("Crest"));
    const auto spectralComplexity_a = std::unique_ptr<standard::Algorithm>(StandardFactory::create ("SpectralComplexity",
        "magnitudeThreshold", settings.spectralComplexity.magnitudeThreshold));
    const auto strongPeakinesses_a = std::unique_ptr<standard::Algorithm>(StandardFactory::create("StrongPeak"));

    std::string const specInputStr  = isPower ? "signal"        : "frame";
    std::string const specOutputStr = isPower ? "powerSpectrum" : "spectrum";

    // std::vector<std::vector<float>> barkBandsVV, BFCCsVV;

    FeatureContainer<vecReal> timbres;

    // Process frame by frame
    int frameCounter = 0;
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(wave);
        frameCutter->output("frame").set(frame);
        frameCutter->compute();

        // check if done
        if (frame.empty()) break;

        // apply windowing
        vecReal windowedFrame;
        windowing->input("frame").set(frame);
        windowing->output("frame").set(windowedFrame);
        windowing->compute();

        // compute spectrum
        vecReal spectrumVec;
        spectrum->input(specInputStr).set(windowedFrame);
        spectrum->output(specOutputStr).set(spectrumVec);
        spectrum->compute();

        // compute BFCC
        vecReal _, bfccVec;
        bfcc->input("spectrum").set(spectrumVec);
        bfcc->output("bands").set(_);
        bfcc->output("bfcc").set(bfccVec);
        bfcc->compute();
        pushBFCCFrame(timbres, bfccVec);

        Real centroid;
        centroid_a->input("array").set(spectrumVec);
        centroid_a->output("centroid").set(centroid);
        centroid_a->compute();
        timbres[Feature_e::SpectralCentroid].push_back(centroid);

        Real decrease;
        decrease_a->input("array").set(spectrumVec);
        decrease_a->output("decrease").set(decrease);
        decrease_a->compute();
        timbres[Feature_e::SpectralDecrease].push_back(decrease);

        Real flatness;
        flatnessDB_a->input("array").set(spectrumVec);
        flatnessDB_a->output("flatnessDB").set(flatness);
        flatnessDB_a->compute();
        timbres[Feature_e::SpectralFlatness].push_back(flatness);

        Real crest;
        crest_a->input("array").set(spectrumVec);
        crest_a->output("crest").set(crest);
        crest_a->compute();
        timbres[Feature_e::SpectralCrest].push_back(crest);

        Real spectralComplexity;
        spectralComplexity_a->input("spectrum").set(spectrumVec);
        spectralComplexity_a->output("spectralComplexity").set(spectralComplexity);
        spectralComplexity_a->compute();
        timbres[Feature_e::SpectralComplexity].push_back(spectralComplexity);

        Real strongPeak;
        strongPeakinesses_a->input("spectrum").set(spectrumVec);
        strongPeakinesses_a->output("strongPeak").set(strongPeak);
        strongPeakinesses_a->compute();
        timbres[Feature_e::StrongPeak].push_back(strongPeak);

        frameCounter++;
    }

    assert(!timbres.bfccs().empty());
    assert(!timbres.bfccs()[0].empty());
    const size_t expected_len = timbres.features[0].size();
    assert(std::ranges::all_of(
        timbres.features.begin(),
        timbres.features.begin() + NumTimbralFeatures,
        [expected_len](const auto &v)
    {
        return (v.size() == expected_len);
    }));

    return timbres;
}

vecVecReal PCA(vecVecReal const &V, int num_features_out){
    const std::string namespaceIn {"data"};
    const std::string namespaceOut {"pca"};

    standard::Algorithm* PCA = nvs::analysis::StandardFactory::create("PCA",
                                                                      "dimensions", num_features_out,
                                                                      "namespaceIn", namespaceIn,
                                                                      "namespaceOut", namespaceOut);

    Pool inPool, outPool;
    for (auto v : V){
        inPool.add(namespaceIn, v);
    }

    PCA->input("poolIn").set(inPool);
    PCA->output("poolOut").set(outPool);
    PCA->compute();

    auto const &vecRealPool = outPool.getVectorRealPool();
    vecVecReal PCAmat = vecRealPool.at(namespaceOut);

    return PCAmat;
}

std::pair<Real, Real> calculateRangeOfDimension(vecReal const &V){
    Real min {std::numeric_limits<Real>::max()};
    Real max {std::numeric_limits<Real>::lowest()};

    for (const float val : V){
        if (val < min){
            min = val;
        }
        if (val > max){
            max = val;
        }
    }

    return std::make_pair(min, max);
}

std::pair<Real, Real> calculateRangeOfDimension(vecVecReal const &V, const size_t dim){
    Real min {std::numeric_limits<Real>::max()};
    Real max {std::numeric_limits<Real>::lowest()};

    for (auto const &v : V){
        auto const val = v[dim];
        if (val < min){
            min = val;
        }
        if (val > max){
            max = val;
        }
    }

    return std::make_pair(min, max);
}

Real calculateNormalizationMultiplier(const std::pair<Real, Real> &range){
    return 1.f / std::max(std::abs(range.first), std::abs(range.second));
}

}
