//
// Created by Nicholas Solem on 5/23/26.
//
#include "StringAxiom.h"
#include "juce_utils.h"
#include "../Source/lib/Settings/SettingsPresets.h"
#include "../Source/lib/Settings/ModernSettingsTypes.h"

template<auto N, auto end, typename F>
constexpr void constexpr_for(F&& f) {
    if constexpr (N < end) {
        f(std::integral_constant<decltype(N), N>{});
        constexpr_for<N + 1, end>(std::forward<F>(f));
    }
}

int main(void) {
    using namespace nvs;
    namespace mod = analysis::modern;
    namespace ax = axiom::tsn;

//======================================================================================================================
    const auto settingsVT = util::loadValueTreeFromJSON(analysis::systemDefaultSettingsPreset);
    const auto vtStr = util::valueTreeToXmlStringSafe(settingsVT);
    std::cout << vtStr << std::endl;

    mod::AnalysisSettings_t analysisSettings;
    const auto printAnalysisSetting = [&analysisSettings](const auto i) {
        const auto v = analysisSettings.get<i>().value;
        std::cout << analysisSettings.get<i>().name << ": " << v << std::endl;
    };

    std::cout << "before loading: " << std::endl;
    constexpr_for<0, mod::AnalysisSettings_t::numSettings> (printAnalysisSetting);

    analysisSettings.fromValueTree(settingsVT);
    std::cout << "after loading: " << std::endl;
    constexpr_for<0, mod::AnalysisSettings_t::numSettings> (printAnalysisSetting);

    return 0;
}