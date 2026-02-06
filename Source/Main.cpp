#include <JuceHeader.h>

#include "AnalysisProgram.h"
#include "ConversionProgram.h"
#include "./version.h"
#include "juce_utils.h"

int main (const int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI juceInit;
    ConsoleApplication app;

    app.addHelpCommand ("--help|-h", "TSN Analyzer - Audio timbre space analysis tool", true);
    app.addVersionCommand ("--version|-v", "version: " + juce::String(LIB_VERSION));

    app.addCommand({
        "--convert|-c",
        "--convert <input_file> <output_file>",
        "Converts a .json file to .tsb file or vice versa.",
        "Converts a .json file to .tsb (binary, losslessly compressed) file or vice versa. json is human-readable " + newLine +
        "but a fairly large file, while tsb is only machine-readable but much smaller in size. The only downside to " + newLine +
            "latter is the inability to read the file. ",
        conversionProgram
    });

    app.addCommand ({
        "--analyze|-a",
        "--analyze <input_file>",
        "Analyzes the audio file and extracts timbre features",
        "This application analyzes an input audio file by splitting it into either events or uniformly-spaced frames, then analyzing each event/frame in terms of pitch, loudness, and timbral features.",
        mainAnalysisProgram
    });

    return app.findAndRunCommand (argc, argv);
}