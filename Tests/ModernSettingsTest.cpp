//
// Created by Nicholas Solem on 5/22/26.
//

#include "ModernSettings.h"
#include <catch2/catch_all.hpp>

#include "juce_utils.h"
#include "../cmake-build-release/_deps/fmt-src/include/fmt/format.h"

namespace nvs::test {

TEST_CASE("Setting default value", "[RangedSetting]")
{
    constexpr char name[] = "myFloat";
    analysis::modern::RangedSetting<double, 0.5, name, -1.0, 2.0> s;

    // constexpr char *wontCompileName1 = "wontCompile";
    // analysis::modern::RangedSetting<double, 0.5, wontCompileName1, -1.0, 2.0> wontCompile1;

    // constexpr std::string_view wontCompileName2 {"wontCompile"};
    // analysis::modern::RangedSetting<double, 0.5, wontCompileName2, -1.0, 2.0> wontCompile2;

    REQUIRE(s.value == 0.5f);
    REQUIRE(s.defaultValue == 0.5f);
    REQUIRE(std::string(s.name) == "myFloat");
    REQUIRE(s.minValue == 0.0f);
    REQUIRE(s.maxValue == 1.0f);
}

TEST_CASE("To and from ValueTree", "[SettingsGroup]")
{
    using namespace analysis::modern;
    using ValueTree = juce::ValueTree;
    constexpr bool printout = false;

    using Group = SettingsGroup<"Group",
        RangedSetting<double, 0.1, "s1", -1.0, 2.0>,
        BoolSetting<true, "s2">,
        ChoiceSetting<"a", "s3", "a", "b", "c">
    >;

    const Group group;
    ValueTree vt("vt");
    group.toValueTree(vt);

    Group group2;
    group2.fromValueTree(vt);

    ValueTree vt2("vt");
    group2.toValueTree(vt2);

    if constexpr (printout) {
        const auto str = util::valueTreeToXmlStringSafe(vt);
        const auto str2 = util::valueTreeToXmlStringSafe(vt2);
        std::cout << str << "\n" << str2 << "\n";
    }

    REQUIRE(vt.isEquivalentTo(vt2));
}

TEST_CASE("", "") {

}

}   // namespace nvs::test