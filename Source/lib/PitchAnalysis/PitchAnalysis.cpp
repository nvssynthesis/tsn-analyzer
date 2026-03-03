//
// Created by Nicholas Solem on 3/3/26.
//

#include "PitchAnalysis.h"
#include "StringAxiom.h"

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

    constexpr int zeroPadding = 2048;

    const auto frameCutter = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("FrameCutter",
                "frameSize",            settings.pitch.frameSize,
                "hopSize",              settings.pitch.hopSize,
                "lastFrameToEndOfFile", true,
                "startFromZero",        true,
                "validFrameThresholdRatio", 0.f
            ));


    const auto windowing = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Windowing",
                "normalized", false,
                "size",        settings.pitch.frameSize,
                "zeroPadding", zeroPadding,
                "type",        settings.analysis.windowingType.toStdString(),
                "zeroPhase",   false
            ));


    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYin",
                "sampleRate",   settings.analysis.sampleRate,
                "frameSize",    settings.pitch.frameSize,
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

    const auto frameCutter = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("FrameCutter",
                "frameSize",            settings.pitch.frameSize,
                "hopSize",              settings.pitch.hopSize,
                "lastFrameToEndOfFile", true,
                "startFromZero",        true,
                "validFrameThresholdRatio", 0.f
            ));


    const auto windowing = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Windowing",
                "normalized", false,
                "size",        settings.pitch.frameSize,
                "zeroPadding", 0, //zeroPadding,
                "type",        "hann",
                "zeroPhase",   false
            ));

    const auto spectrum = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Spectrum",
                "size",  settings.pitch.frameSize
                ));

    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYinFFT",
                "sampleRate",   settings.analysis.sampleRate,
                "frameSize",    settings.pitch.frameSize,
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

}   // namespace nvs::analysis