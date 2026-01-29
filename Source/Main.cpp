#include <JuceHeader.h>

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

int main (const int argc, char* argv[])
{
    ConsoleApplication app;
    StdoutLogger logger;
    Logger::setCurrentLogger(&logger);
    auto print = [](const String& message) {
        Logger::writeToLog(message + newLine);
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

        [print](const ArgumentList& args) {
            if (args.arguments.size() < 2)
            {
                print("Error: Please specify an input file");
                return;
            }

            const File inputFile = args.arguments[1].resolveAsFile();



            print("Analyzing: " + inputFile.getFileName());

            // TODO: analysis code here

            print("Analysis complete!");
        }
    });

    return app.findAndRunCommand (argc, argv);
}