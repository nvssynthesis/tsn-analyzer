//
// Created by Nicholas Solem on 5/23/26.
//
#include "StringAxiom.h"
#include "juce_utils.h"
#include "../Source/lib/Settings/SettingsPresets.h"
#include "../Source/lib/Settings/ModernSettingsTypes.h"

int main(void) {
    using namespace nvs;
    namespace mod = analysis::modern;
    namespace ax = axiom::tsn;

//======================================================================================================================
    const auto settingsVT = util::loadValueTreeFromJSON(analysis::systemDefaultSettingsPreset);
    const auto vtStr = util::valueTreeToXmlStringSafe(settingsVT);
    std::cout << "In ValueTree: " << '\n' << vtStr << std::endl;

    mod::AnalysisSettings_t analysisSettings;
    using Registry = mod::SettingsRegistry<analysis::modern::AnalysisSettings_t>;
    Registry registry;
    registry.fromValueTree(settingsVT);

    const auto outVT = registry.createValueTree();
    const auto outStr = util::valueTreeToXmlStringSafe(outVT);
    std::cout << "Out ValueTree: " << '\n' <<outStr << std::endl;

    return 0;
}