#include <JuceHeader.h>

#include "StringAxiom.h"
#include "ThreadedAnalyzer.h"
#include "Settings.h"
#include "TSNValueTreeUtilities.h"
#include "juce_utils.h"
#include "OnsetAnalysis/OnsetProcessing.h"

struct StdoutLogger final : Logger
{
    ~StdoutLogger() override {
        Logger::setCurrentLogger (nullptr); // otherwise we hit "jassert (currentLogger != this);"
    }
    void logMessage (const String& message) override
    {
        std::puts(message.toRawUTF8());
    }
};

struct AudioFileInfo {
    int64 numSamples;   // all we need for now
    double sampleRate;
    unsigned int bitDepth;
};

AudioFileInfo readIntoBuffer(AudioSampleBuffer &buff, const juce::File &file) {

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

int main (const int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    ConsoleApplication app;
    StdoutLogger logger;
    Logger::setCurrentLogger(&logger);
    auto print = [](const String& message) {
        Logger::writeToLog(message + newLine);
    };

    auto getInputFile = [] (const ArgumentList &args) -> File
    {
        if (args.arguments.size() < 2)
            return {};
        return args.arguments[1].resolveAsExistingFile();
    };

    auto makeSettingsParentTree = [] (double sampleRate, const String &filePath)
    {
        nvs::analysis::AnalyzerSettings settings;
        settings.analysis.sampleRate = sampleRate;
        settings.info.sampleFilePath = filePath;
        settings.analysis.numThreads = 8;

        const auto settingsParentTree = nvs::analysis::createParentTreeFromSettings(settings);
        return settingsParentTree;
    };

    struct AnalyzerResult {
        std::optional<nvs::analysis::TimbreAnalysisResult> timbres {};
        std::shared_ptr<nvs::analysis::OnsetAnalysisResult> onsets {};
    };
    auto runAnalyzer = [print] (const std::span<const float> &channel, const String &fileName, auto &settingsTree) -> AnalyzerResult
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

        print("Analysis thread begun...");
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
    };

    auto mainAnalysisProgram = [print, &getInputFile, &makeSettingsParentTree, &runAnalyzer](const ArgumentList &args) -> void
    {
        const File inputFile = getInputFile(args);
        if (inputFile == juce::File{}) {
            print("Error: Please specify an input file");
            jassertfalse;
            return;
        }

        const auto fileName = inputFile.getFileName();
        print("Opening " + fileName + "...");

        AudioSampleBuffer buffer;
        const auto [numSamples, sampleRate, bitDepth] = readIntoBuffer(buffer, inputFile);

        const auto rp = buffer.getReadPointer(0);
        const std::span channel0(rp, numSamples);

        const auto& audioFileFullAbsPath = inputFile.getFullPathName();
        const auto settingsParentTree = makeSettingsParentTree(sampleRate, audioFileFullAbsPath);
        const auto treeStr = nvs::util::valueTreeToXmlStringSafe(settingsParentTree);
        print(treeStr);

        auto /*cant be const*/ settingsTree = settingsParentTree.getChildWithName(nvs::axiom::tsn::Settings);
        const auto analysisResult = runAnalyzer(channel0, fileName, settingsTree);
        if ((analysisResult.onsets == nullptr) || (analysisResult.timbres == std::nullopt)) {
            DBG("Analysis failed; returning");
            jassertfalse;
            return;
        }

        print("Analysis complete!");
        const auto timbreSpaceRepr = (*analysisResult.timbres).timbreMeasurements;
        const auto onsets = (*analysisResult.onsets).onsets;
        const auto waveformHash = (*analysisResult.timbres).waveformHash;
        // const String waveformHash = nvs::util::hashAudioData(std::vector(channel0.begin(), channel0.end()));


        const auto timbreSpaceVT = nvs::analysis::timbreSpaceReprToVT(timbreSpaceRepr,
            onsets, waveformHash, audioFileFullAbsPath);
        print(nvs::util::valueTreeToXmlStringSafe(timbreSpaceVT));
    };

    app.addHelpCommand ("--help|-h", "TSN Analyzer - Audio timbre space analysis tool", true);
    app.addVersionCommand ("--version|-v", "TSN Analyzer version 0.1.0");

    app.addCommand ({
        "--analyze",
        "--analyze <input_file>",
        "Analyzes the audio file and extracts timbre features",
        "This application analyzes an input audio file by splitting it into either events or " + newLine
        + String("uniformly-spaced frames, then analyzing each event/frame in terms of pitch, loudness, and" + newLine
            + String("timbral features.")),
        mainAnalysisProgram
    });

    return app.findAndRunCommand (argc, argv);
}