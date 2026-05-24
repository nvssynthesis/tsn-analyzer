//
// Created by Nicholas Solem on 5/23/26.
//
#include "StringAxiom.h"
#include "ModernSettings.h"
#include "juce_utils.h"
#include "SettingsPresets.h"

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

    using frameSizeSetting_t = mod::RangedSetting<int, 1024, "frameSize", 4, 16384>;
    using hopSizeSetting_t = mod::RangedSetting<int, 512, "hopSize", 1, 8192>;
    using windowSetting_t = mod::ChoiceSetting<ax::blackmanharris92, ax::windowingType,
        ax::hann, ax::hamming, ax::hannnsgcq, ax::triangular, ax::square,
        ax::blackmanharris62, ax::blackmanharris70,
        ax::blackmanharris74, ax::blackmanharris74>;
    using analysisSettings_t = mod::SettingsGroup<"Analysis", frameSizeSetting_t, hopSizeSetting_t, windowSetting_t>;
//======================================================================================================================
    const auto settingsVT = util::loadValueTreeFromJSON(analysis::systemDefaultSettingsPreset);
    const auto vtStr = util::valueTreeToXmlStringSafe(settingsVT);
    std::cout << vtStr << std::endl;

    analysisSettings_t analysisSettings;
    const auto printAnalysisSetting = [&analysisSettings](const auto i) {
        const auto v = analysisSettings.get<i>().value;
        std::cout << analysisSettings.get<i>().name << ": " << v << std::endl;
    };

    std::cout << "before loading: " << std::endl;
    constexpr_for<0, analysisSettings_t::numSettings> (printAnalysisSetting);

    analysisSettings.fromValueTree(settingsVT);
    std::cout << "after loading: " << std::endl;
    constexpr_for<0, analysisSettings_t::numSettings> (printAnalysisSetting);

    return 0;
}