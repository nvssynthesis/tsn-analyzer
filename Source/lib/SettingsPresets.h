//
// Created by Nicholas Solem on 2/7/26.
//

#pragma once
#include "Settings.h"
#include "juce_core/juce_core.h"

namespace nvs::analysis {
using File = juce::File;

const auto settingsPresetLocation = File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("tsn_analyzer");

const auto systemDefaultSettingsPreset = settingsPresetLocation.getChildFile("default_config.json");


}   // nnamespace nvs::analysis