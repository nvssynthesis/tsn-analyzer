#include "AnalysisProgram.h"
#include "lib/StringAxiom.h"
#include "lib/OnsetAnalysis/OnsetProcessing.h"
#include "juce_utils.h"
#include "ProgramUtils.h"
#include "SettingsPresets.h"
#include "./lib/config.h"

using namespace juce;

static AudioFileInfo readIntoBuffer(AudioSampleBuffer &buff, const juce::File &file)
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

static ValueTree makeSettingsParentTree(double sampleRate, const String &filePath)
{
    nvs::analysis::AnalyzerSettings settings;
    settings.analysis.sampleRate = sampleRate;
    settings.info.sampleFilePath = filePath;
    settings.analysis.numThreads = 8;

    const auto settingsParentTree = nvs::analysis::createParentTreeFromSettings(settings);
    return settingsParentTree;
}
static ValueTree makeSettingsParentTree(const ValueTree settingsTree, const double sampleRate, const String &filePath)
{
    ValueTree settingsParentTree("Root");

    ValueTree fileInfoTree(nvs::axiom::tsn::FileInfo);
    fileInfoTree.setProperty(nvs::axiom::tsn::sampleFilePath, filePath, nullptr);
    fileInfoTree.setProperty(nvs::axiom::tsn::sampleRate, sampleRate, nullptr);
    settingsParentTree.appendChild(fileInfoTree, nullptr);

    settingsParentTree.addChild(settingsTree, -1, nullptr);
    return settingsParentTree;
}
static AnalyzerResult runAnalyzer(const std::span<const float> &channel, const String &audioFileFullAbsolutePath, auto &settingsTree)
{
    nvs::analysis::ThreadedAnalyzer analyzer;
    analyzer.updateStoredAudioAndSettings(channel, audioFileFullAbsolutePath, settingsTree, true);
    if (!analyzer.startThread(Thread::Priority::normal)) {
        Logger::writeToLog("Failed to start analysis thread\n");
        return {};
    }

    Logger::writeToLog("Analysis thread begun...");
    while (analyzer.isThreadRunning()) {
        Thread::sleep(100);
    }
    jassert(analyzer.onsetsReady() && analyzer.timbreAnalysisReady());

    auto timbreSpaceRepr = analyzer.shareTimbreSpaceRepresentation();
    auto onsets = analyzer.shareOnsetAnalysis();
    auto pacmap = analyzer.sharePacmapResult();
    return AnalyzerResult{
        .timbres = std::move(timbreSpaceRepr),
        .onsets = std::move(onsets),
        .pacmap = std::move(pacmap),
        .settingsHash = analyzer.getSettingsHash()
    };
}

void mainAnalysisProgram(const ArgumentList &args)
{
    if (args.arguments.size() < 3) {
        Logger::writeToLog("Not enough arguments");
        return;
    }
    const File inputAudioFile = getInputFile(args, File::getCurrentWorkingDirectory());
    if (inputAudioFile == File{}) {
        Logger::writeToLog("Error: Please specify an input file");
        jassertfalse;
        return;
    }

    Logger::writeToLog("Opening " + inputAudioFile.getFileName() + "...");

    AudioSampleBuffer buffer;
    const auto audioFileInfo = readIntoBuffer(buffer, inputAudioFile);

    const auto rp = buffer.getReadPointer(0);
    const std::span channel0(rp, audioFileInfo.numSamples);

    const auto& audioFileFullAbsPath = inputAudioFile.getFullPathName();

    struct SettingsStuff {
        ValueTree settingsParentTree {};
        File settingsFile {};
    };
    const auto [settingsParentTree, settingsFile] = [&args, &audioFileInfo, &audioFileFullAbsPath]() -> SettingsStuff
    {
        SettingsStuff _settingsStuff;
        if (const auto settingsStr = args.getValueForOption("--settings|-s");
            !settingsStr.isEmpty())
        {
            _settingsStuff.settingsFile = asAbsPathOrWithinDirectory(settingsStr, nvs::analysis::settingsPresetLocation);
            const auto settingsVT = nvs::analysis::loadValueTreeFromFile(_settingsStuff.settingsFile);
            _settingsStuff.settingsParentTree = makeSettingsParentTree(settingsVT, audioFileInfo.sampleRate, audioFileFullAbsPath);
            return _settingsStuff;
        }
        // ~/Library/tsn_analyzer/default_settings.json
        _settingsStuff.settingsFile = nvs::analysis::systemDefaultSettingsPreset;
        const auto settingsVT = nvs::analysis::loadValueTreeFromFile(_settingsStuff.settingsFile);
        _settingsStuff.settingsParentTree = makeSettingsParentTree(settingsVT, audioFileInfo.sampleRate, audioFileFullAbsPath);
        return _settingsStuff;
    }();
    const auto treeStr = nvs::util::valueTreeToXmlStringSafe(settingsParentTree);

    auto /*can't be const*/ settingsTree = settingsParentTree.getChildWithName(nvs::axiom::tsn::Settings);
    const auto settingsTreeOriginal = settingsTree.createCopy();
    const auto analysisResult = runAnalyzer(channel0, audioFileFullAbsPath, settingsTree);
    if (analysisResult.onsets == nullptr || analysisResult.timbres == nullptr) {
        Logger::writeToLog("Analysis failed; returning");
        jassertfalse;
        return;
    }
    if (!settingsTree.isEquivalentTo( settingsTreeOriginal)) {
        // tree changed; give opportunity to overwrite original file
        Logger::writeToLog("Settings tree updated. Overwrite original? (y/N)");
        if (const auto response = checkForYesNoResponse()) {
            Logger::writeToLog("Updating settings file");

            nvs::util::saveValueTreeToJSON(settingsParentTree.getChildWithName(nvs::axiom::tsn::Settings), settingsFile);

        } else {
            Logger::writeToLog("Settings file not updated");
        }
    }

    Logger::writeToLog("Analysis complete!");
    const auto timbreSpaceRepr = analysisResult.timbres->timbreMeasurements;
    const auto onsets = analysisResult.onsets->onsets;
    const std::shared_ptr<nvs::analysis::PacmapResult> pacmap = analysisResult.pacmap;
    const auto waveformHash = analysisResult.timbres->waveformHash;

    const auto timbreSpaceVT = nvs::analysis::timbreSpaceReprToVT(timbreSpaceRepr, onsets,
        pacmap == nullptr ? nullptr : &pacmap->pacmapMatrix_);

    jassert((analysisResult.onsets->audioFileAbsPath == analysisResult.timbres->audioFileAbsPath) &&
            (analysisResult.onsets->audioFileAbsPath == audioFileFullAbsPath));


    if (args.containsOption("--print|-p")) {
        Logger::writeToLog(nvs::util::valueTreeToXmlStringSafe(timbreSpaceVT));
        return;
    }
    if (const File outAnalysisFile =
        getOutputAnalysisFile(args,
        nvs::config::analysisFilesLocation,
        analysisResult.settingsHash, true);
        outAnalysisFile == File{})
    {
        return; // cancelled or error, already reported
    }
    else {
        Logger::writeToLog("Writing to " + outAnalysisFile.getFullPathName());
        const auto superTree = nvs::analysis::makeSuperTree(
            timbreSpaceVT,
            audioFileFullAbsPath,
            audioFileInfo.sampleRate,
            waveformHash,
            analysisResult.settingsHash,
            settingsTree);

        if (outAnalysisFile.getFileExtension() == ".json") {
            nvs::util::saveValueTreeToJSON(superTree, outAnalysisFile);
        }
        else if (outAnalysisFile.getFileExtension() == ".tsb") {
            nvs::util::saveValueTreeToBinary(superTree, outAnalysisFile);
        }
    }
}
