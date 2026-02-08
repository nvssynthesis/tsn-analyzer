//
// Created by Nicholas Solem on 2/7/26.
//

#pragma once
#include "juce_core/juce_core.h"

namespace nvs::config {
using juce::File;
/*
 authorship information, default locations to use for finding/saving analysis and settings files, etc
*/
const auto configFileLocation {
    File::getSpecialLocation(File::SpecialLocationType::userHomeDirectory).getChildFile(".config").getChildFile("tsn_analyzer")
};

const auto analysisFilesLocation {
    File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("tsn_analyzer")
};


}
