#include <JuceHeader.h>

#include "AnalysisProgram.h"
#include "ConversionProgram.h"
#include "SettingsPrograms.h"
#include "./version.h"
#include "juce_utils.h"

int main (const int argc, char* argv[]) {
    /// TODO: -n for dry run, --verbose when relevant

    ScopedJuceInitialiser_GUI juceInit;
    ConsoleApplication app;

    app.addHelpCommand ("--help|-h", "TSN Analyzer - Audio timbre space analysis tool", true);
    app.addVersionCommand ("--version|-v", "version: " + juce::String(LIB_VERSION));

    app.addCommand({"--settings-path|-p",
        "--settings-path|-p",
        "Prints the path to the settings directory.",
        "Prints the path to the settings directory.",
        printSettingsPath
    });

    app.addCommand({"--settings-default|-d",
        "--settings-default|-d",
        "Prints the current default settings preset.",
        "Prints current default settings.",
        printCurrentSettings
    });

    app.addCommand({"--create-settings|-s",
        "--create-settings|-s",
        "Creates new settings from default.",
        "Creates a new settings preset from the current default settings.",
        createSettingsPresetFromDefault
    });

    app.addCommand({
        "--convert|-c",
        "--convert <input_file>",
        "Converts a .json file to .tsb file or vice versa.",
        "Converts a .json file to .tsb (binary, losslessly compressed) file or vice versa. json is human-" + newLine +
        "readable but a fairly large file, while tsb is only machine-readable but much smaller in size. The only downside" + newLine +
            "to latter is the inability to read the file. ",
        conversionProgram
    });

    app.addCommand ({
        "--analyze|-a",
        "--analyze <input_file> [-s <settings_file>] [-o <output_file>]",
        "Analyzes the audio file and extracts timbre features",
        "This application analyzes an input audio file by splitting it into either events or " + newLine +
            "uniformly-spaced frames, then analyzing each event/frame in terms of pitch, loudness, and timbral features." + newLine +
                "If no <output_file> is supplied, analyze will simply print the resulting analysis to the console.",
        mainAnalysisProgram
    });

    return app.findAndRunCommand (argc, argv);
}