//
// Created by Nicholas Solem on 5/22/26.
//

#include <catch2/catch_all.hpp>
#include <catch2/catch_approx.hpp>

#include "../Source/lib/Settings/ModernSettings.h"
#include "juce_utils.h"
#include "../cmake-build-release/_deps/fmt-src/include/fmt/format.h"

namespace nvs::test {

TEST_CASE("Setting default value", "[RangedSetting]")
{
    using namespace analysis::modern;

    constexpr char name[] = "myFloat";
    analysis::modern::RangedSetting<SInfo<name>, double, 0.5, -1.0, 2.0> s;

    // constexpr char *wontCompileName1 = "wontCompile";
    // analysis::modern::RangedSetting<SInfo<wontCompileName1>, double, 0.5, -1.0, 2.0> wontCompile1;

    // constexpr std::string_view wontCompileName2 {"wontCompile"};
    // analysis::modern::RangedSetting<SInfo<wontCompileName2>, double, 0.5, -1.0, 2.0> wontCompile2;

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
        RangedSetting<SInfo<"s1", "">, double, 0.1, -1.0, 2.0>, // explicit about blank default
        BoolSetting<SInfo<"s2">, true>, // default tooltip==""
        ChoiceSetting<SInfo<"s3", "tooltip">, "a", "a", "b", "c">
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
    SECTION("get/set values") {
        Group group;
        REQUIRE(group.getValue("") == std::nullopt);
        REQUIRE(std::get<double>(group.getValue("s1").value()) == Catch::Approx(0.1));
        REQUIRE(std::get<bool>(group.getValue("s2").value()) == true);
        REQUIRE(std::get<juce::String>(group.getValue("s3").value()) == "a");

        group.setValue("s1", 0.2);
        REQUIRE(std::get<double>(group.getValue("s1").value()) == Catch::Approx(0.2));
        group.setValue("s2", false);
        REQUIRE(std::get<bool>(group.getValue("s2").value()) == false);
        group.setValue("s3", "b");
        REQUIRE(std::get<juce::String>(group.getValue("s3").value()) == "b");

        REQUIRE(group.getFloatValue("s2") == std::nullopt);
        const auto v = group.getFloatValue("s1").value();
        REQUIRE(v == Catch::Approx(0.2));
        REQUIRE(group.getStringValue("s1") == std::nullopt);
        REQUIRE(group.getStringValue("s3").value() == "b");

        REQUIRE(group.setFloatValue("s2", 100.0) == false);
        REQUIRE(group.setFloatValue("s1", 0.3) == true);
        REQUIRE(group.getFloatValue("s1").value() == Catch::Approx(0.3));
        REQUIRE(group.setBoolValue("", true) == false);
        REQUIRE(group.setBoolValue("s2", true) == true);
        REQUIRE(group.setStringValue("s1", "nope") == false);

#pragma message("Advanced: could require the following to actually fail, since \"yes\" is not one of the choices")
        REQUIRE(group.setStringValue("s3", "yes") == true);
        REQUIRE(group.getStringValue("s3").value() == "yes");
    }
    SECTION("resetDefaults") {
        Group group;
        const auto spec = group.getFloatSpec("s1");
        const auto defaultVal = spec.value().defaultValue;
        REQUIRE(group.getFloatValue("s1") == defaultVal);
        group.setFloatValue("s1", 10.5);
        REQUIRE(group.getFloatValue("s1") != defaultVal);
        group.resetToDefaults();
        REQUIRE(group.getFloatValue("s1") == defaultVal);
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
    SECTION("get spec") {
        Group group;
        auto fSpec = group.getSpec<RangedSettingsSpec<double>>("");
        REQUIRE(fSpec.has_value() == false);
        fSpec = group.getSpec<RangedSettingsSpec<double>>("s1");
        REQUIRE(fSpec.has_value());
        REQUIRE(fSpec.value().range.start == -1.0);
        REQUIRE(fSpec.value().range.end == 2.0);

        auto bSpec = group.getBoolSpec("");
        REQUIRE(bSpec.has_value() == false);
        bSpec = group.getBoolSpec("s2");
        REQUIRE(bSpec.has_value());
        REQUIRE(bSpec.value().defaultValue == true);
    }
}

}   // namespace nvs::test