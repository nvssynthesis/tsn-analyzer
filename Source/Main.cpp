#include <JuceHeader.h>

#include "AnalysisProgram.h"
#include "./version.h"

void conversionProgram(const ArgumentList &args) {
    if (args.arguments.size() < 2) {
        DBG("Not enough arguments");
        return;
    }
    const auto inFile = getInputFile(args);
    const auto inFileExt = inFile.getFileExtension();

    const auto outFile = [&args, &inFile, &inFileExt]() -> File {
        if (args.arguments.size() < 3) {
            // const String outFn =  inFile.getFileNameWithoutExtension();
            if (inFileExt == ".tsb") {
                return inFile.withFileExtension(".json");
            }
            if (inFileExt == ".json") {
                return inFile.withFileExtension(".tsb");
            }
            DBG("extensions must be either .json or .tsb");
            return File();
        }
        return getOutputFile(args);
    }();
    if (outFile == File()) {
        DBG("invalid extension; returning...");
        return;
    }


    const auto outFileExt = outFile.getFileExtension();

    if ( (inFileExt != ".tsb" && inFileExt != ".json") || (outFileExt != ".tsb" && outFileExt != ".json") )
    {
        DBG("extensions must be either .json or .tsb");
        return;
    }
    if (inFileExt == outFileExt) {
        DBG("extensions must be complimentary (e.g. if input extension is tsb, output should be json)");
        return;
    }

}

int main (const int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;
    ConsoleApplication app;

    app.addHelpCommand ("--help|-h", "TSN Analyzer - Audio timbre space analysis tool", true);
    app.addVersionCommand ("--version|-v", "version: " + juce::String(LIB_VERSION));

    app.addCommand({
        "--convert",
        "--convert <input_file> <output_file>",
        "Converts a .json file to .tsb file or vice versa.",
        "Converts a .json file to .tsb (binary, losslessly compressed) file or vice versa. json is human-readable " + newLine +
        "but a fairly large file, while tsb is only machine-readable but much smaller in size. The only downside to " + newLine +
            "latter is the inability to read the file. ",
        conversionProgram
    });

    app.addCommand ({
        "--analyze",
        "--analyze <input_file>",
        "Analyzes the audio file and extracts timbre features",
        "This application analyzes an input audio file by splitting it into either events or uniformly-spaced frames, then analyzing each event/frame in terms of pitch, loudness, and timbral features.",
        mainAnalysisProgram
    });

    return app.findAndRunCommand (argc, argv);
}