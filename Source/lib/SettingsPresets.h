//
// Created by Nicholas Solem on 2/7/26.
//

#pragma once
#include "Settings.h"
#include "juce_core/juce_core.h"
#include "juce_utils.h"

namespace nvs::analysis {

using juce::ValueTree;
using juce::File;

const auto settingsPresetLocation = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("tsn_analyzer");
const auto systemDefaultSettingsPreset = settingsPresetLocation.getChildFile("default_settings.json");
const auto customPresetsDirectory = settingsPresetLocation.getChildFile("presets");


inline ValueTree loadValueTreeFromFile(const File &vtFile) {
    if (vtFile.getFileExtension() == ".tsb") {
        return nvs::util::loadValueTreeFromBinary(vtFile);
    }
    if (vtFile.getFileExtension() == ".json") {
        return nvs::util::loadValueTreeFromJSON(vtFile);
    }
    std::cerr << "loadValueTreeFromFile: Error loading file: " << vtFile.getFileName() << std::endl;
    return {};
}


}   // namespace nvs::analysis