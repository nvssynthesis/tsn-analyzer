#pragma once
#include "SettingsPresets.h"

inline void printConfigPath(const ArgumentList &) {
    using nvs::analysis::settingsPresetLocation;

    if (const auto result = settingsPresetLocation.createDirectory();
        !result)
    {
        std::cerr << result.getErrorMessage() << std::endl;
        std::cerr << "Failed to create analyzer config directory at " << settingsPresetLocation.getFullPathName() << std::endl;
        return;
    }
    std::cout << settingsPresetLocation.getFullPathName() << std::endl;
}

inline void printValueTreeFile(const juce::File &valueTreeFile) {
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

inline void printCurrentConfig(const ArgumentList &args) {
    using nvs::analysis::settingsPresetLocation;
    using nvs::analysis::systemDefaultSettingsPreset;

    if (const auto result = settingsPresetLocation.createDirectory();
        !result)
    {
        std::cerr << result.getErrorMessage() << std::endl;
        std::cerr << "Failed to create analyzer config directory at " << settingsPresetLocation.getFullPathName() << std::endl;
        return;
    }
    if (systemDefaultSettingsPreset.exists()) {
        // print current defaults from file
        printValueTreeFile(systemDefaultSettingsPreset);
    }
    else if (const auto result = systemDefaultSettingsPreset.create();
        !result)
    {
        std::cerr << result.getErrorMessage() << std::endl;
        std::cerr << "Failed to create analyzer config directory at " << systemDefaultSettingsPreset.getFullPathName() << std::endl;
        return;
    }
    // if we're here, the default file has been properly created
    // const nvs::analysis::AnalyzerSettings settings; // default settings
    // convert settings object to value tree
    ValueTree vt("Settings");
    nvs::analysis::initializeSettingsBranches(vt);
    std::cout << nvs::util::valueTreeToXmlStringSafe(vt) << std::endl;
}