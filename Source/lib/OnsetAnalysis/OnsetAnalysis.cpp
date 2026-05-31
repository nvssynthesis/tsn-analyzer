/*
  ==============================================================================

    OnsetAnalysis.cpp
    Created: 14 Jun 2023 12:16:36pm
    Author:  Nicholas Solem

  ==============================================================================
*/

#include "OnsetAnalysis.h"
#include "essentia/scheduler/network.h"

/** TODO:
 consolidate onsetsInSeconds with onsetAnalysis.
 The trouble is that there was a problem figuring out how to use essentia's streaming factory (instead of standard) with
 the Onsets algorithm. So that function has to reference a different factory.
 */

namespace nvs::analysis {

vecReal makeSweptSine(Real const low, Real const high, size_t const len, Real const sampleRate){
	vecReal freqSweep(len, 0.f);
	for (size_t i = 0; i < len; ++i){
		const Real x = (high - low) * (static_cast<Real>(i) / static_cast<Real>(len)) + low;
		freqSweep[i] = std::sin(2.f * 3.14159265f * (x / sampleRate));
	}

	return freqSweep;
}
static vecReal getWeights(const modern::AnalyzerSettingsRegistry &settings) {
    namespace ax = axiom::tsn;
    const auto& group = settings.get<modern::OnsetSettings>();
    return {
        static_cast<float>(group.getFloatValue(ax::weight_hfc)),
        static_cast<float>(group.getFloatValue(ax::weight_complex)),
        static_cast<float>(group.getFloatValue(ax::weight_complexPhase)),
        static_cast<float>(group.getFloatValue(ax::weight_flux)),
        static_cast<float>(group.getFloatValue(ax::weight_rms)),
        static_cast<float>(group.getFloatValue(ax::weight_novelty))
    };
}

array2dReal calculateOnsetsMatrix(const vecReal &waveform,
                      const double sampleRate,
					  const modern::AnalyzerSettingsRegistry &settings,
					  RunLoopStatus& rls,
					  const ShouldExitFn &shouldExit)
{
    namespace ax = axiom::tsn;
    const auto &analysisSettings = settings.get<modern::AnalysisSettings>();
	const auto input_sr          = sampleRate;
	assert(0.0 < input_sr);
    constexpr auto internal_sr   = 44100.0f;
	const auto frameSize      = std::min(std::max(512, analysisSettings.getIntValue(ax::frameSize)), 2048);
	constexpr auto hopSize       = 512;

	auto *inVec = new vectorInput(&waveform);   // NOLINT – network takes ownership

	Algorithm* resampler	= StreamingFactory::create("Resample",
											 "inputSampleRate", input_sr,
											 "outputSampleRate", internal_sr,
											 "quality",	2);	/* quality: SRC_SINC_FASTEST
															 from enum {
																	   SRC_SINC_BEST_QUALITY       = 0,
																	   SRC_SINC_MEDIUM_QUALITY     = 1,
																	   SRC_SINC_FASTEST            = 2,
																	   SRC_ZERO_ORDER_HOLD         = 3,
																	   SRC_LINEAR                  = 4
																   } ;
															 */

#pragma message("This FrameCutter config could use a bit more thought. startFromZero should ideally be true, but then the beginning of file fades in with window.")
	// ============ Main processing chain (variable frameSize) ============
	Algorithm* frameCutter  = StreamingFactory::create("FrameCutter",
											 "frameSize", frameSize,
											 "hopSize", hopSize,
											 "startFromZero", false,
											 "lastFrameToEndOfFile", true,
											 "validFrameThresholdRatio", 0.0f);

	Algorithm* windowingToFFT = StreamingFactory::create("Windowing",
											 "normalized", true,
											 "size", frameSize,
											 "zeroPhase", false,
											 "zeroPadding", frameSize,
											 "type", "hamming");

	Algorithm* FFT			= StreamingFactory::create("FFT",
											 "size", frameSize);

	Algorithm* carToPol		= StreamingFactory::create("CartesianToPolar");

    // ============ Connect beginning of chain (independent of which onset detectors are used) ============
    *inVec >> resampler->input("signal");
    resampler->output("signal") >> frameCutter->input("signal");
    frameCutter->output("frame") >> windowingToFFT->input("frame");
    windowingToFFT->output("frame") >> FFT->input("frame");
    FFT->output("fft") >> carToPol->input("complex");


	// ============ Connect individual onset detection algorithms ============
    vecReal onsetDetVecHFC, onsetDetVecComplex, onsetDetVecComplexPhase, onsetDetVecFlux, onsetDetVecRms, onsetDetVecNovelty;
    std::array<std::reference_wrapper<vecReal>, 6> detectionRefs {
        onsetDetVecHFC,
        onsetDetVecComplex,
        onsetDetVecComplexPhase,
        onsetDetVecFlux,
        onsetDetVecRms,
        onsetDetVecNovelty
    };

    const auto weights = getWeights(settings);

    if (const auto weightSum = std::accumulate(weights.begin(), weights.end(), 0.f);
        weightSum == 0.f)
    {
        jassertfalse;
        // handle case where user asked for cumulative weight of 0
    }

    const auto &onsetSettings = settings.get<modern::OnsetSettings>();
    if (0.f < onsetSettings.getFloatValue(ax::weight_hfc)) {
        Algorithm* onsetDetectionHfc = StreamingFactory::create("OnsetDetection",
                                                "method", "hfc",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionHfc->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionHfc->input("phase");
        auto *onsetDetsHFC = new vectorOutput(&onsetDetVecHFC);     // NOLINT – network takes ownership
        onsetDetectionHfc->output("onsetDetection") >> *onsetDetsHFC;
    }
    if (0.f < onsetSettings.getFloatValue(ax::weight_complex)) {
        Algorithm* onsetDetectionComplex = StreamingFactory::create("OnsetDetection",
                                                "method", "complex",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionComplex->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionComplex->input("phase");
        auto *onsetDetsComplex = new vectorOutput(&onsetDetVecComplex);     // NOLINT – network takes ownership
        onsetDetectionComplex->output("onsetDetection") >> *onsetDetsComplex;
    }
    if (0.f < onsetSettings.getFloatValue(ax::weight_complexPhase)) {
        Algorithm* onsetDetectionComplexPhase = StreamingFactory::create("OnsetDetection",
                                                "method", "complex_phase",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionComplexPhase->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionComplexPhase->input("phase");
        auto *onsetDetsComplexPhase = new vectorOutput(&onsetDetVecComplexPhase);       // NOLINT – network takes ownership
        onsetDetectionComplexPhase->output("onsetDetection") >> *onsetDetsComplexPhase;
    }
    if (0.f < onsetSettings.getFloatValue(ax::weight_flux)) {
        Algorithm* onsetDetectionFlux = StreamingFactory::create("OnsetDetection",
                                                "method", "flux",
                                                   "sampleRate", internal_sr);
        carToPol->output("magnitude") >> onsetDetectionFlux->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionFlux->input("phase");
        auto *onsetDetsFlux = new vectorOutput(&onsetDetVecFlux);       // NOLINT – network takes ownership
        onsetDetectionFlux->output("onsetDetection") >> *onsetDetsFlux;
    }
    if (0.f < onsetSettings.getFloatValue(ax::weight_rms)) {
        Algorithm* onsetDetectionRms = StreamingFactory::create("OnsetDetection",
                                                "method", "rms",
                                                   "sampleRate", internal_sr);
        auto *onsetDetsRms = new vectorOutput(&onsetDetVecRms);     // NOLINT – network takes ownership
        carToPol->output("magnitude") >> onsetDetectionRms->input("spectrum");
        carToPol->output("phase")	>> onsetDetectionRms->input("phase");
        onsetDetectionRms->output("onsetDetection") >> *onsetDetsRms;
    }

    std::vector<std::vector<vecReal>> spectrogramHolder {}; // created in main function scope for lifetime (VectorOutput doesn't seem to take ownership)
    if (0.f < onsetSettings.getFloatValue(ax::weight_novelty)) {
        // this scope is all just to PREPARE the necessary spectrogram input FOR NoveltyCurve!
        VectorOutput<std::vector<vecReal>> *spectrumAccumOutput = new VectorOutput<std::vector<vecReal>>(&spectrogramHolder);   // NOLINT – network takes ownership
        Algorithm* spectrumFrameAccumulator = StreamingFactory::create("VectorRealAccumulator");
        carToPol->output("magnitude")               >>      spectrumFrameAccumulator->input("data");
        carToPol->output("phase")                   >>      essentia::streaming::DEVNULL;
        spectrumFrameAccumulator->output("array")   >>      *spectrumAccumOutput;
    }

	Network n(inVec);
	n.runPrepare();
	rls.set(0.0);
#pragma message("We should be able to know how many network steps and thus set rls with onset progress.")
	rls.set("Computing onset matrix...");
	while (n.runStep()){
		if (shouldExit()) {
			return {};
		}
	}
	rls.set(1.0);
	n.clear();

    VectorOutput<Real> *noveltyAccumOutput = new VectorOutput(&onsetDetVecNovelty);
    try {
        if (0.f < onsetSettings.getFloatValue(ax::weight_novelty)) {
            jassert(spectrogramHolder.size() == 1);
            const vecVecReal &spectrogram = spectrogramHolder[0];

            constexpr Real frameRate = internal_sr / hopSize;

            Algorithm* noveltyCurve = StreamingFactory::create("NoveltyCurve",
                "frameRate", frameRate,
                "normalize", false);

            auto *spectrogramVecInput = new vectorInputCumulative(&spectrogram);       // NOLINT – network takes ownership


            *spectrogramVecInput                        >>   noveltyCurve->input("frequencyBands");
            noveltyCurve->output("novelty")    >> *noveltyAccumOutput;

            Network n2(spectrogramVecInput);
            n2.runPrepare();
            n2.run();

            n2.clear();

            jassert(onsetDetVecNovelty.size() > 0);
        }
    }
    catch (const EssentiaException &e) {
        std::cerr << e.what() << '\n';
        jassertfalse;
    }
    // this bit just takes all the detection outputs and makes them constant-size (which should only not happen if some of them had a weight of 0)
    const auto correctSizedVec = std::ranges::max_element(detectionRefs,
                                                          [](const vecReal &v0, const vecReal &v1)
                                                          {
                                                              return v0.size() < v1.size();
                                                          });
    const size_t correctSize = correctSizedVec->get().size();

    jassert (0 < correctSize);
    for (auto d : detectionRefs) {
        auto &ref = d.get();
        if (ref.empty()) {
            ref = vecReal(correctSize, 0.f);
        }
        if (ref.size() < correctSize) {
            const size_t diff = correctSize - ref.size();
            // insert zeros on end
            ref.insert(ref.end(), diff, 0.f);
        }
    }

    for (auto d : detectionRefs) {
        jassert (d.get().size() == correctSize);
    }

	TNT::Array2D<essentia::Real> onsetsMatrix(detectionRefs.size(), static_cast<int>(correctSize));
	for (size_t j = 0; j < correctSize; ++j){
		onsetsMatrix[0][j] = onsetDetVecHFC[j];
		onsetsMatrix[1][j] = onsetDetVecComplex[j];
		onsetsMatrix[2][j] = onsetDetVecComplexPhase[j];
		onsetsMatrix[3][j] = onsetDetVecFlux[j];
		onsetsMatrix[4][j] = onsetDetVecRms[j];
	    onsetsMatrix[5][j] = onsetDetVecNovelty[j];
	}
	return onsetsMatrix;
}

#pragma message("make this work with StreamingFactory")
vecReal calculateOnsetsInSeconds(const array2dReal &onsetAnalysisMatrix,
								 const modern::AnalyzerSettingsRegistry &settings)
{
	/* assuming that the onsetAnalysisMatrix was derived from the above onsetAnalysis,
	 (which is beyond likely in this codebase because it's not so trivial to construct that array2dReal),
	 the sample rate of the signal will have already been converted to 44100 before the analysis.
	 */

	constexpr float frameRate = 44100.f / 512.f;

    namespace ax = axiom::tsn;
    const auto &onsetSettings = settings.get<modern::OnsetSettings>();
	standard::Algorithm* onsetDetectionSeconds = StandardFactory::create (
		"Onsets",
		  "frameRate",       frameRate,
		  "silenceThreshold",onsetSettings.getFloatValue(ax::silenceThreshold),
		  "alpha",           onsetSettings.getFloatValue(ax::alpha), // proportion of the mean included to reject smaller peaks-filters very short onsets
		  "delay",           onsetSettings.getIntValue(ax::numFrames_shortOnsetFilter) // number of frames used to compute the threshold-size of short-onset filter
	);

    const vecReal weights = getWeights(settings);

	vecReal onsets;
	onsetDetectionSeconds->input("detections").set(onsetAnalysisMatrix);
	onsetDetectionSeconds->input("weights").set(weights);
	onsetDetectionSeconds->output("onsets").set(onsets);

	onsetDetectionSeconds->compute();

	for (size_t i = 1; i < onsets.size(); ++i) {
		assert(onsets[i - 1] < onsets[i]);
	}

	return onsets;
}

vecVecReal splitWaveIntoEvents(
    const vecReal &wave,
    const double sampleRate,
    const vecReal &onsetsInSeconds,
    const modern::AnalyzerSettingsRegistry &settings,
    RunLoopStatus& rls, const ShouldExitFn &shouldExit)
{
	size_t const numOnsets {onsetsInSeconds.size()};
	assert(numOnsets);
	if (numOnsets == 1){	// only 1 event
		vecVecReal retVal{ vecReal(wave.begin(), wave.end()) };
		return retVal;
	}
	vecReal endTimes(numOnsets);
	std::copy(onsetsInSeconds.begin() + 1, onsetsInSeconds.end(), endTimes.begin());
	assert(onsetsInSeconds[1] == endTimes[0]);

    const auto anSettings = settings.getGroupTyped<const modern::AnalysisSettings>(axiom::tsn::Analysis).value().get();
	assert (sampleRate > 8000.f);

	Real const endOfFile = static_cast<Real>(wave.size() - 1) / sampleRate;
	endTimes.back() = endOfFile;
	assert(*(onsetsInSeconds.end() - 1) == *(endTimes.end() - 2));

	Algorithm* slicer = StreamingFactory::create("Slicer",
									   "timeUnits", "seconds",
									   "sampleRate", sampleRate,
									   "startTimes", onsetsInSeconds,
									   "endTimes", endTimes);

	auto *waveInput = new vectorInput(&wave);                                               // NOLINT – network takes ownership
	vecVecReal waveEvents;
	vectorOutputCumulative *waveEventsOutput = new vectorOutputCumulative(&waveEvents);     // NOLINT – network takes ownership

	*waveInput >> slicer->input("audio");
	slicer->output("frame") >> *waveEventsOutput;

	Network n(waveInput);
	n.runPrepare();
	while (n.runStep()){
		if (shouldExit()){
			break;
		}
	}
	n.clear();

	assert(!waveEvents.empty());

    namespace ax = axiom::tsn;
	for (auto & waveEvent : waveEvents){
		size_t currentLength = waveEvent.size();
	    const auto splitSettings = settings.get<const modern::SplitSettings>();

		const size_t fadeInSamps = std::min(static_cast<size_t>(splitSettings.getIntValue(ax::fadeInSamps)), currentLength);
		for (size_t j = 0; j < fadeInSamps; ++j){
			waveEvent[j] = waveEvent[j] * (static_cast<Real>(j) / static_cast<Real>(fadeInSamps));
		}
		const size_t fadeOutSamps = std::min(static_cast<size_t>(splitSettings.getIntValue(ax::fadeOutSamps)), currentLength);
		for (size_t j = 0; j < fadeOutSamps; ++j){
			const size_t currentIdx = (currentLength - 1) - j;
			waveEvent[currentIdx] = waveEvent[currentIdx] * (static_cast<Real>(j) / static_cast<Real>(fadeOutSamps));
		}
	}

	return waveEvents;
}

void writeWav(const vecReal &wave,
    const double sampleRate,
    const std::string_view name,
	const modern::AnalyzerSettingsRegistry &settings,
	RunLoopStatus& rls,
	const ShouldExitFn &shouldExit)
{
    namespace ax = axiom::tsn;
	jassert (sampleRate > 20000.f);
	Algorithm* writer = StreamingFactory::create("MonoWriter",
									   "filename", std::string(name) + ".wav",
									   "format", "wav",
									   "sampleRate", sampleRate);
	vectorInput *waveInput = new vectorInput(&wave);        // NOLINT – network takes ownership
	*waveInput >> writer->input("audio");

	Network n(waveInput);
	n.runPrepare();
	while (n.runStep()){
		if (shouldExit()){
			break;
		}
	}
	n.clear();
}
void writeWavs(const vecVecReal &waves, const double sampleRate, const std::string_view defName,
			   const modern::AnalyzerSettingsRegistry &settings,
			   RunLoopStatus& rls,
			   const ShouldExitFn &shouldExit)
{
	int idx = 0;
	std::string name(defName);
    for (auto const &wave : waves){
		std::string strIdx = std::to_string(idx++);
		name += strIdx;					// add index to name

		writeWav(wave, sampleRate, name, settings, rls, shouldExit);

		name.erase(name.back() - strIdx.length(), name.back());	// remove index from name
	}
}

} // namespace nvs::analysis
