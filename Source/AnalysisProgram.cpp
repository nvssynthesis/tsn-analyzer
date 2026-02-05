#include "AnalysisProgram.h"
#include "lib/StringAxiom.h"
#include "lib/OnsetAnalysis/OnsetProcessing.h"
#include "juce_utils.h"
#include "ProgramUtils.h"

using namespace juce;

AudioFileInfo readIntoBuffer(AudioSampleBuffer &buff, const juce::File &file)
{
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    const auto reader = std::unique_ptr<AudioFormatReader>( formatManager.createReaderFor(file));
    if (reader == nullptr) {
        DBG("Failed to open file " + file.getFileName());
        return {};
    }

    const auto numSamps = reader->lengthInSamples;
    if (constexpr auto maxLength = std::numeric_limits<int>::max();
        numSamps > maxLength)
    {
        DBG("Number of samples is greater than the maximum allowed length (" + juce::String{numSamps} + " samples)");
        return {};
    }
    buff.setSize(1, static_cast<int>(numSamps));
    jassert (static_cast<int>(numSamps) <= buff.getNumSamples());
    reader->read(&buff, 0, static_cast<int>(numSamps), 0, true, true);

    return {
        .numSamples = numSamps,
        .sampleRate = reader->sampleRate,
        .bitDepth = reader->bitsPerSample
    };
}

ValueTree makeSettingsParentTree(double sampleRate, const String &filePath)
{
    nvs::analysis::AnalyzerSettings settings;
    settings.analysis.sampleRate = sampleRate;
    settings.info.sampleFilePath = filePath;
    settings.analysis.numThreads = 8;

    const auto settingsParentTree = nvs::analysis::createParentTreeFromSettings(settings);
    return settingsParentTree;
}

AnalyzerResult runAnalyzer(const std::span<const float> &channel, const String &audioFileFullAbsolutePath, auto &settingsTree)
{
    if (!nvs::analysis::verifySettingsStructure(settingsTree)) {
        DBG("Settings structure verification failed");
        return {};
    }

    nvs::analysis::ThreadedAnalyzer analyzer;
    analyzer.updateStoredAudio(channel, audioFileFullAbsolutePath);
    analyzer.updateSettings(settingsTree, true);
    if (!analyzer.startThread(Thread::Priority::normal)) {
        DBG("Failed to start analysis thread\n");
        return {};
    }

    Logger::writeToLog("Analysis thread begun...");
    while (analyzer.isThreadRunning()) {
        Thread::sleep(100);
    }
    jassert(analyzer.onsetsReady() && analyzer.timbreAnalysisReady());

    auto timbreSpaceRepr = analyzer.stealTimbreSpaceRepresentation();
    auto onsets = analyzer.shareOnsetAnalysis();
    return AnalyzerResult{
        .timbres = std::move(timbreSpaceRepr),
        .onsets = std::move(onsets),
        .settingsHash = analyzer.getSettingsHash()
    };
}

void mainAnalysisProgram(const ArgumentList &args)
{
    const File inputAudioFile = getInputFile(args);
    if (inputAudioFile == juce::File{}) {
        DBG("Error: Please specify an input file");
        jassertfalse;
        return;
    }

    Logger::writeToLog("Opening " + inputAudioFile.getFileName() + "...");

    AudioSampleBuffer buffer;
    const auto [numSamples, sampleRate, bitDepth] = readIntoBuffer(buffer, inputAudioFile);

    const auto rp = buffer.getReadPointer(0);
    const std::span channel0(rp, numSamples);

    const auto& audioFileFullAbsPath = inputAudioFile.getFullPathName();
    const auto settingsParentTree = makeSettingsParentTree(sampleRate, audioFileFullAbsPath);
    const auto treeStr = nvs::util::valueTreeToXmlStringSafe(settingsParentTree);
    Logger::writeToLog(treeStr);

    auto /*can't be const*/ settingsTree = settingsParentTree.getChildWithName(nvs::axiom::tsn::Settings);
    const auto analysisResult = runAnalyzer(channel0, audioFileFullAbsPath, settingsTree);
    if ((analysisResult.onsets == nullptr) || (analysisResult.timbres == std::nullopt)) {
        DBG("Analysis failed; returning");
        jassertfalse;
        return;
    }

    Logger::writeToLog("Analysis complete!");
    const auto timbreSpaceRepr = analysisResult.timbres->timbreMeasurements;
    const auto onsets = analysisResult.onsets->onsets;
    const auto waveformHash = analysisResult.timbres->waveformHash;

    const auto timbreSpaceVT = nvs::analysis::timbreSpaceReprToVT(timbreSpaceRepr, onsets);

    jassert((analysisResult.onsets->audioFileAbsPath == analysisResult.timbres->audioFileAbsPath) &&
            (analysisResult.onsets->audioFileAbsPath == audioFileFullAbsPath));

    if (const File outFile = getOutputFile(args);
        outFile == File{})
    {
        Logger::writeToLog("No output file argument; printing analysis tree...\n");
        Logger::writeToLog(nvs::util::valueTreeToXmlStringSafe(timbreSpaceVT));
    }
    else {
        Logger::writeToLog("Writing to " + outFile.getFullPathName());
        const auto superTree = nvs::analysis::makeSuperTree(
            timbreSpaceVT,
            audioFileFullAbsPath,
            sampleRate,
            waveformHash,
            analysisResult.settingsHash,
            settingsTree);

        if (outFile.getFileExtension() == ".json") {
            nvs::util::saveValueTreeToJSON(superTree, outFile);
        }
        else if (outFile.getFileExtension() == ".tsb") {
            nvs::util::saveValueTreeToBinary(superTree, outFile);
        }
    }
}
