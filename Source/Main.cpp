#include <JuceHeader.h>

#include "AnalysisProgram.h"
#include "ConversionProgram.h"
#include "SettingsPrograms.h"
#include "./version.h"
#include "lib/version.h"

#include "juce_utils.h"

int main (const int argc, char* argv[]) {
    /// TODO: -n for dry run, --verbose when relevant
    /// TODO: global config file in ~/.config
    ///
    ScopedJuceInitialiser_GUI juceInit;
    ConsoleApplication app;

    app.addHelpCommand ("--help|-h", "TSN Analyzer - Audio timbre space analysis tool", true);

    app.addVersionCommand ("--version|-v",
        "tsn_analyzer lib version: " + String(LIB_VERSION) + newLine +
        "tsn_analyze program version: " + String(PROGRAM_VERSION));

    app.addCommand({"--print-path|-p",
        "--print-path|-p [--settings|-s] [--config|-c]",
        "Prints one of the paths used for the functionality of tsn_analyzer lib and tsn_analyze app.",
        "Prints one of the paths used for the functionality of tsn_analyzer lib and tsn_analyze app." + newLine +
            "The [--settings|-s] flag will print the path of the directory containing all preset files." + newLine +
                "The [--config|-c] flag (TODO) will print the path of the singular config file, which is responsible for holding" + newLine +
                    "authorship information, default locations to use for finding/saving analysis and settings files, etc.",
        printPath
    });

    app.addCommand({"--settings-default|-d",
        "--settings-default|-d",
        "Prints the current default settings preset.",
        "Prints current default settings.",
        printCurrentSettings
    });

    app.addCommand({"--settings|-s",
        "--settings|-s <output_file>",  // in the future there might be modes within '--settings' e.g. -c for 'create', -i for 'interactive', etc.
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