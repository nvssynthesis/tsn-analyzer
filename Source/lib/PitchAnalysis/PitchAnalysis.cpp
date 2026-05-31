//
// Created by Nicholas Solem on 3/3/26.
//

#include "PitchAnalysis.h"
#include "StringAxiom.h"

namespace nvs::analysis {
namespace {

PitchesAndConfidences processFrequenciesAndConfidences(
    vecReal &&frequencies,
    const vecReal &confidences,
    const double sampleRate,
    const modern::AnalyzerSettingsRegistry &settings)
{
    // convert frequency to pitch
    vecReal pitches = std::move(frequencies);
    assert(pitches.size() == confidences.size());

    // if we move to c++23, replace with zip iteration
    std::transform(pitches.begin(), pitches.end(), // first1, last1
        confidences.begin(),    // first2
        pitches.begin(),    // output
        [&settings, sampleRate](const float pitch, const float confidence) {
            if ((pitch <= 0.f) || (pitch >= sampleRate * 0.5)) {
                return -100.0f;
            }
            const auto pSettings = settings.get<modern::PitchSettings>();
            namespace ax = axiom::tsn;
            if (pSettings.getBoolValue(ax::replace_dismal_confidences_with_constant)) {
                if (confidence <= pSettings.getFloatValue(ax::dismal_confidence_threshold)) {
                    return static_cast<float>(pSettings.getFloatValue(ax::dismal_replacement_constant));
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

PitchesAndConfidences calculatePitchesEssentiaYin(
    const vecReal &waveEvent, const double sampleRate, modern::AnalyzerSettingsRegistry const& settings)
{
    constexpr int zeroPadding = 2048;

    const auto pSettings = settings.get<modern::PitchSettings>();
    namespace ax = axiom::tsn;
    const auto frameCutter = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("FrameCutter",
                "frameSize",            pSettings.getIntValue(ax::frameSize),
                "hopSize",              pSettings.getIntValue(ax::hopSize),
                "lastFrameToEndOfFile", true,
                "startFromZero",        true,
                "validFrameThresholdRatio", 0.f
            ));


    const auto windowing = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Windowing",
                "normalized", false,
                "size",        pSettings.getIntValue(ax::frameSize),
                "zeroPadding", zeroPadding,
                "type",        settings.getString(ax::Analysis, ax::windowingType).value().toStdString(),
                "zeroPhase",   false
            ));


    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYin",
                "sampleRate",   sampleRate,
                "frameSize",    pSettings.getIntValue(ax::frameSize),
                "interpolate",  pSettings.getBoolValue(ax::interpolate),
                "maxFrequency", pSettings.getFloatValue(ax::maxFrequency),
                "minFrequency", pSettings.getFloatValue(ax::minFrequency),
                "tolerance",    pSettings.getFloatValue(ax::tolerance)
            ));

    vecReal frequencies, confidences; // accumulate results manually
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(waveEvent);
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
    return processFrequenciesAndConfidences(std::move(frequencies), confidences, sampleRate, settings);
}

PitchesAndConfidences calculatePitchesEssentiaYinFFT(
    const vecReal &waveEvent, const double sampleRate, modern::AnalyzerSettingsRegistry const& settings)
{
    const auto pSettings = settings.get<modern::PitchSettings>();
    namespace ax = axiom::tsn;
    const auto frameCutter = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("FrameCutter",
                "frameSize",            pSettings.getIntValue(ax::frameSize),
                "hopSize",              pSettings.getIntValue(ax::hopSize),
                "lastFrameToEndOfFile", true,
                "startFromZero",        true,
                "validFrameThresholdRatio", 0.f
            ));


    const auto windowing = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Windowing",
                "normalized", false,
                "size",        pSettings.getIntValue(ax::frameSize),
                "zeroPadding", 0, //zeroPadding,
                "type",        "hann",
                "zeroPhase",   false
            ));

    const auto spectrum = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("Spectrum",
                "size",  pSettings.getIntValue(ax::frameSize)
                ));

    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYinFFT",
                "sampleRate",   sampleRate,
                "frameSize",    pSettings.getIntValue(ax::frameSize),
                "interpolate",  pSettings.getBoolValue(ax::interpolate),
                "maxFrequency", pSettings.getFloatValue(ax::maxFrequency),
                "minFrequency", pSettings.getFloatValue(ax::minFrequency),
                "tolerance",    pSettings.getFloatValue(ax::tolerance),
                "weighting", "custom"   // {custom, A, B, C, D, Z}
            ));

    vecReal frequencies, confidences; // accumulate results manually
    while (true) {
        vecReal frame;

        // get next frame
        frameCutter->input("signal").set(waveEvent);
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

    return processFrequenciesAndConfidences(std::move(frequencies), confidences, sampleRate, settings);
}

PitchesAndConfidences calculatePitchesEssentiaProbabilisticYin(
    const vecReal &waveEvent, const double sampleRate,
    modern::AnalyzerSettingsRegistry const& settings)
{
    const auto pSettings = settings.get<modern::PitchSettings>();
    namespace ax = axiom::tsn;
    const auto outputUnvoiced = [&pSettings]() -> std::string {
        if (pSettings.getBoolValue(ax::replace_dismal_confidences_with_constant)) {
            //  {zero, abs, negative}
            const auto dismal_replacement_constant = pSettings.getStringValue(ax::dismal_replacement_constant);
            return dismal_replacement_constant.toStdString();
        }
        return "abs";
    }();

    const auto pitchDet = std::unique_ptr<standard::Algorithm>(
        StandardFactory::create ("PitchYinProbabilistic",
            "sampleRate",   sampleRate,
            "frameSize",    pSettings.getIntValue(ax::frameSize),
            "hopSize",      pSettings.getIntValue(ax::hopSize),
            "lowRMSThreshold", pSettings.getIntValue(ax::lowRMSThreshold),
            "preciseTime",  pSettings.getIntValue(ax::preciseTime),
            "outputUnvoiced", outputUnvoiced  // {"zero", "abs", "negative"}
        ));

    vecReal frequencies, confidences;

    pitchDet->input("signal").set(waveEvent);
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
            [&pSettings](const float pitch, const float confidence) {
                if (pitch <= 0.f) {
                    return 0.0f;
                }
        // NOTE: PitchYinProbabilistic already takes care of this EXCEPT the nyquist setting
                if (pSettings.getBoolValue(ax::replace_dismal_confidences_with_constant)) {
                    if (confidence <= pSettings.getFloatValue(ax::dismal_confidence_threshold)) {
                        return static_cast<float>(pSettings.getFloatValue(ax::dismal_replacement_constant));
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

PitchesAndConfidences calculatePitchesAndConfidences(const vecReal &waveEvent, const double sampleRate,
                                                      modern::AnalyzerSettingsRegistry const& settings)
{
    auto const algo = settings.getString(axiom::tsn::Pitch, axiom::tsn::pitchDetectionAlgorithm).value().toStdString();
    try {
        if (algo == axiom::tsn::yin) {
            return calculatePitchesEssentiaYin (waveEvent, sampleRate, settings);
        }
        if (algo == axiom::tsn::yinFFT) {
            return calculatePitchesEssentiaYinFFT(waveEvent, sampleRate, settings);
        }
        if (algo == axiom::tsn::pYin) {
            return calculatePitchesEssentiaProbabilisticYin (waveEvent, sampleRate, settings);
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

