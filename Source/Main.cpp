#include <JuceHeader.h>

#include "StringAxiom.h"
#include "ThreadedAnalyzer.h"
#include "Settings.h"

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

    auto mainAnalysisProgram = [print](const ArgumentList &args) -> void
    {
        if (args.arguments.size() < 2)
        {
            print("Error: Please specify an input file");
            jassertfalse;
            return;
        }

        const File inputFile = args.arguments[1].resolveAsExistingFile();
        if (inputFile == juce::File{}) {
            DBG("File invalid\n");
            jassertfalse;
            return;
        }
        const auto fileName = inputFile.getFileName();

        print("Opening " + fileName + "...");

        AudioSampleBuffer buffer;
        const auto [numSamples, sampleRate, bitDepth] = readIntoBuffer(buffer, inputFile);

        const auto rp = buffer.getReadPointer(0);
        const std::span channel0(rp, numSamples);


        nvs::analysis::AnalyzerSettings settings;
        settings.analysis.sampleRate = sampleRate;
        settings.info.sampleFilePath = inputFile.getFullPathName();
        settings.analysis.numThreads = 8;

        const auto settingsParentTree = nvs::analysis::createParentTreeFromSettings(settings);
        auto settingsTree = settingsParentTree.getChildWithName(nvs::axiom::Settings);
        if (!nvs::analysis::verifySettingsStructure(settingsTree)) {
            DBG("Settings structure verification failed");
            jassertfalse;
            return;
        }

        nvs::analysis::ThreadedAnalyzer analyzer;
        analyzer.updateStoredAudio(channel0, fileName);
        analyzer.updateSettings(settingsTree, true);
        if (analyzer.startThread(Thread::Priority::normal)) {
            print("Analysis thread begun...");
        }
        else {
            DBG("Failed to start analysis thread\n");
            jassertfalse;
            return;
        }

        while (analyzer.isThreadRunning()) {
            Thread::sleep(100);
        }

        print("Analysis complete!");
        return;
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