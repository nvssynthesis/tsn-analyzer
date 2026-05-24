//
// Created by Nicholas Solem on 5/22/26.
//

#include "../Source/lib/Settings/ModernSettings.h"
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

TEST_CASE("SettingsGroup", "[SettingsGroup]")
{
    using namespace analysis::modern;
    using ValueTree = juce::ValueTree;
    constexpr bool printout = false;
//========================================================
    using Group = SettingsGroup<"Group",
        RangedSetting<double, 0.1, "s1", -1.0, 2.0>,
        BoolSetting<true, "s2">,
        ChoiceSetting<"a", "s3", "a", "b", "c">
    >;

    SECTION("toFromValueTree") {
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
    SECTION("resetDefaults") {
        Group group;
        REQUIRE(group.get<0>().value == group.get<0>().defaultValue);
        auto &s1 = group.get<0>();
        const auto s1NewValue = s1.value + 0.5;
        s1.value = s1NewValue;
        REQUIRE(s1.value != s1.defaultValue);
        group.resetToDefaults();
        REQUIRE(s1.value == s1.defaultValue);
    }
    SECTION("public members") {
        Group group;
        REQUIRE(std::string(group.groupName) == "Group");
        REQUIRE(group.numSettings == 3);
    }
    SECTION("getSpecs") {
        Group group;
        const std::map<juce::String, AnySpec>& specs = group.getSpecs();
        REQUIRE(specs.size() == 3);
        REQUIRE(specs.find("s1") != specs.end());
        REQUIRE(specs.find("s2") != specs.end());
        REQUIRE(specs.find("s3") != specs.end());
        REQUIRE(specs.find("s1")->first == "s1");
        AnySpec spec1 = specs.find("s1")->second;
        REQUIRE(std::holds_alternative<RangedSettingsSpec<double>>(spec1));
    }
}


}   // namespace nvs::test