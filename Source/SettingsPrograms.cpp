#include "SettingsPrograms.h"
#include "juce_utils.h"
#include "ProgramUtils.h"

using juce::String;
using juce::Array;
using juce::Result;


void printPath(const ArgumentList &args) {

    bool some_flag_found = false;
    if (foundFlag(args, {"-s", "--settings"}))
    {
        some_flag_found = true;
        using nvs::analysis::settingsPresetLocation;

        if (const auto result = settingsPresetLocation.createDirectory();
            !result)
        {
            std::cerr << result.getErrorMessage() << std::endl;
            std::cerr << "Failed to create analyzer settings directory at " << settingsPresetLocation.getFullPathName() << std::endl;
            return;
        }
        std::cout << "settings preset location: " <<
            settingsPresetLocation.getFullPathName() << std::endl;
    }
    if (foundFlag(args, {"-c", "--config"}))
    {
        some_flag_found = true;
        std::cout << "tsn_analyzer config location: " << "\n";
        std::cerr << "NOT YET IMPLEMENTED" << std::endl;
    }
    if (!some_flag_found) {
        std::cerr << "no element to print was found." << std::endl;
        std::cout << "optional flags for --print-path: [--settings|-s] [--config|-c]" << std::endl;
    }
}

void printValueTreeFile(const juce::File &valueTreeFile) {
    const ValueTree vt = [](const File &f) -> ValueTree {
        if (f.getFileExtension() == ".tsb") {
            return nvs::util::loadValueTreeFromBinary(f);
        }
        if (f.getFileExtension() == ".json") {
            return nvs::util::loadValueTreeFromJSON(f);
        }
        jassertfalse;
        return {};
    }(valueTreeFile);
    std::cout << nvs::util::valueTreeToXmlStringSafe(vt) << std::endl;
}

ValueTree createDefaultPresetFile() {
    // returns the created file's value tree in case we need it further, to avoid redundant I/O
    using nvs::analysis::systemDefaultSettingsPreset;
    if (const Result res = systemDefaultSettingsPreset.create();
        !res)
    {
        std::cerr << res.getErrorMessage() << std::endl;
        std::cerr << "Failed to create analyzer settings directory at " << systemDefaultSettingsPreset.getFullPathName() << std::endl;
    }
    // if we're here, the default file has been properly created
    // convert settings object to value tree
    ValueTree vt("Settings");
    nvs::analysis::initializeSettingsBranches(vt);
    nvs::util::saveValueTreeToJSON(vt, systemDefaultSettingsPreset.getFullPathName());
    return vt;
}

void printCurrentSettings(const ArgumentList &args) {
    using nvs::analysis::settingsPresetLocation;
    using nvs::analysis::systemDefaultSettingsPreset;

    if (const auto result = settingsPresetLocation.createDirectory();
        !result)
    {
        std::cerr << result.getErrorMessage() << std::endl;
        std::cerr << "Failed to create analyzer settings directory at " << settingsPresetLocation.getFullPathName() << std::endl;
        return;
    }
    if (systemDefaultSettingsPreset.exists()) {
        // print current defaults from file
        printValueTreeFile(systemDefaultSettingsPreset);
    }
    else {
        createDefaultPresetFile();
    }
    printValueTreeFile(systemDefaultSettingsPreset);
}

void createSettingsPresetFromDefault(const ArgumentList &args) {
    // query args for new preset name
    /// TODO: also get optional entries <author> and <preset description>

    if (args.arguments.size() < 2) {
        std::cerr << "Not enough arguments";
        return;
    }

    const auto &presetsDir = nvs::analysis::customPresetsDirectory;
    if (const Result presetDirCreated = presetsDir.createDirectory(); !presetDirCreated) {
        std::cerr << presetDirCreated.getErrorMessage() << std::endl;
    }


    const File outputSettingsFile = presetsDir.getChildFile(args.arguments[1].resolveAsFile().getFileName());

    // check if same file name already exists
    if (outputSettingsFile.existsAsFile()) {
        std::cerr << "File already exists; returning..." << std::endl;
        return;
    }
    if ((outputSettingsFile.getFileExtension() != ".tsb") && (outputSettingsFile.getFileExtension() != ".json")) {
        std::cerr << "Output settings file must end with tsb or json." << std::endl;
        return;
    }

    { // check for name match regardless of extension as well
        const auto fnWithoutExt = outputSettingsFile.getFileNameWithoutExtension();
        Array<File> directoryFiles = presetsDir.findChildFiles(
            File::TypesOfFileToFind::findFilesAndDirectories,
            true,
            "*",
            File::FollowSymlinks::noCycles);
        Array<File> similarFileNames {};
        for (auto const & f : directoryFiles) {
            if (const auto existingSimilarFilenameWithoutExt = f.getFileNameWithoutExtension(); fnWithoutExt == existingSimilarFilenameWithoutExt) {
                // PROBLEM: there exists some file within our presets directory with a name matching our new preset's
                // name. Inquire with user if we should proceed anyway.
                similarFileNames.add(f.getFullPathName());
            }
        }
        if (const auto &numSimilarFns = similarFileNames.size(); numSimilarFns > 0)
        {
            if (numSimilarFns == 1) {
                std::cout << "WARNING: " << numSimilarFns << " similar file name found: " << std::endl;
            } else {
                jassert(numSimilarFns >= 2);
                std::cout << "WARNING: " << numSimilarFns << " similar file names found: " << std::endl;
            }

            for (auto const & f : similarFileNames) {
                std::cout << f.getFullPathName() << std::endl;
            }
            { // get response on whether to create anyway
                std::cout << "Create new preset file at " << fnWithoutExt << " anyway? (y/N)" << std::endl;
                if (checkForYesNoResponse()) {
                    // yes create new settings preset file even though similar name exists
                    std::cout << "proceeding with creation of new preset file" << std::endl;
                } else {
                    // do not create new settings preset file
                    std::cout << "aborting..." << std::endl;
                    return;
                }
            }
        }
    }

    const ValueTree defaultVT = []() {
        if (const File &defaultPreset = nvs::analysis::systemDefaultSettingsPreset; defaultPreset.existsAsFile()) {
            return nvs::analysis::loadValueTreeFromFile(defaultPreset);
        }
        return createDefaultPresetFile();
    }();

    if (outputSettingsFile.getFileExtension() == ".tsb") {
        // convert default settings to .tsb file and save it in presets
        nvs::util::saveValueTreeToBinary(defaultVT, outputSettingsFile);
        std::cout << "New settings file created at " << outputSettingsFile.getFullPathName() << std::endl;
    } else if (outputSettingsFile.getFileExtension() == ".json") {
        nvs::util::saveValueTreeToJSON(defaultVT, outputSettingsFile);
        std::cout << "New settings file created at " << outputSettingsFile.getFullPathName() << std::endl;
    }
    else {
        std::cerr << "File must have extension .tsb or .json" << std::endl;
        return;
    }
}