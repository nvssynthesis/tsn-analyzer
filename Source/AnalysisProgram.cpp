#include "AnalysisProgram.h"
#include "lib/StringAxiom.h"
#include "lib/OnsetAnalysis/OnsetProcessing.h"
#include "juce_utils.h"

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

File getInputFile(const ArgumentList &args)
{
    if (args.arguments.size() < 2)
        return {};
    return args.arguments[1].resolveAsExistingFile();
}

File getOutputFile(const ArgumentList &args)
{
    if (args.arguments.size() < 3) {
        return {};
    }
    auto outputFile = File::getCurrentWorkingDirectory()
                          .getChildFile(args.arguments[2].text);

    if (const bool forceOverwrite = args.containsOption("--force|-f");
        outputFile.existsAsFile() && !forceOverwrite)
    {
        std::cout << "Warning: Output file '" << outputFile.getFullPathName()
                  << "' already exists.\n";
        std::cout << "Overwrite? (y/N): ";

        std::string response;
        std::getline(std::cin, response);

        if (response.empty() || (response[0] != 'y' && response[0] != 'Y'))
        {
            std::cout << "Operation cancelled.\n";
            return {};
        }
    }

    return outputFile;
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

AnalyzerResult runAnalyzer(const std::span<const float> &channel, const String &fileName, auto &settingsTree)
{
    if (!nvs::analysis::verifySettingsStructure(settingsTree)) {
        DBG("Settings structure verification failed");
        return {};
    }

    nvs::analysis::ThreadedAnalyzer analyzer;
    analyzer.updateStoredAudio(channel, fileName);
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
        .onsets = std::move(onsets)
    };
}

void mainAnalysisProgram(const ArgumentList &args)
{
    const File inputFile = getInputFile(args);
    if (inputFile == juce::File{}) {
        DBG("Error: Please specify an input file");
        jassertfalse;
        return;
    }

    const auto fileName = inputFile.getFileName();
    Logger::writeToLog("Opening " + fileName + "...");

    AudioSampleBuffer buffer;
    const auto [numSamples, sampleRate, bitDepth] = readIntoBuffer(buffer, inputFile);

    const auto rp = buffer.getReadPointer(0);
    const std::span channel0(rp, numSamples);

    const auto& audioFileFullAbsPath = inputFile.getFullPathName();
    const auto settingsParentTree = makeSettingsParentTree(sampleRate, audioFileFullAbsPath);
    const auto treeStr = nvs::util::valueTreeToXmlStringSafe(settingsParentTree);
    Logger::writeToLog(treeStr);

    auto /*cant be const*/ settingsTree = settingsParentTree.getChildWithName(nvs::axiom::tsn::Settings);
    const auto analysisResult = runAnalyzer(channel0, fileName, settingsTree);
    if ((analysisResult.onsets == nullptr) || (analysisResult.timbres == std::nullopt)) {
        DBG("Analysis failed; returning");
        jassertfalse;
        return;
    }

    Logger::writeToLog("Analysis complete!");
    const auto timbreSpaceRepr = (*analysisResult.timbres).timbreMeasurements;
    const auto onsets = (*analysisResult.onsets).onsets;
    const auto waveformHash = (*analysisResult.timbres).waveformHash;

    const auto timbreSpaceVT = nvs::analysis::timbreSpaceReprToVT(timbreSpaceRepr,
        onsets, waveformHash, audioFileFullAbsPath);

    if (const File outFile = getOutputFile(args);
        outFile == juce::File{})
    {
        Logger::writeToLog(nvs::util::valueTreeToXmlStringSafe(timbreSpaceVT));
    }
    else {
        Logger::writeToLog("Writing to " + outFile.getFullPathName());
        nvs::util::saveValueTreeToJSON(timbreSpaceVT, outFile);
    }
}
