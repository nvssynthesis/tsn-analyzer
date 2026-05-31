//
// Created by Nicholas Solem on 5/22/26.
//

#include <catch2/catch_all.hpp>

#include "juce_utils.h"
#include "Settings/ModernSettings.h"
#include "Settings/ModernSettingsTypes.h"

namespace nvs::test {

TEST_CASE("SettingsRegistry", "[SettingsRegistry]") {
    using namespace analysis::modern;
    namespace ax = axiom::tsn;

    const AnalyzerSettingsRegistry registry1;

    const auto g1 = registry1.getGroupTyped<const AnalysisSettings>(ax::Analysis).value().get();
    REQUIRE(std::is_const_v<std::remove_reference_t<decltype(g1)>>);

    AnalyzerSettingsRegistry registry2;

    auto g2 = registry2.getGroupTyped<AnalysisSettings>(ax::Analysis).value().get();
    REQUIRE(!std::is_const_v<std::remove_reference_t<decltype(g2)>>);
}
TEST_CASE("SettingsRegistry-Simplest API", "[SettingsRegistry]") {
    using namespace analysis::modern;

    AnalyzerSettingsRegistry registry;
    auto frameSize = registry.get<AnalysisSettings>().getIntValue(ax::frameSize);
    PitchSettings& pitch    = registry.get<PitchSettings>();

    std::cout <<"algo: " << pitch.getStringValue(ax::pitchDetectionAlgorithm) << std::endl;

    registry.setSetting(ax::Pitch, ax::pitchDetectionAlgorithm, ax::pYin);

    std::cout <<"algo: " << pitch.getStringValue(ax::pitchDetectionAlgorithm) << std::endl;

    REQUIRE(pitch.setValue(ax::minFrequency, 1000.0));
    REQUIRE(pitch.getFloatValue(ax::minFrequency) == Catch::Approx(1000.0));

    REQUIRE(false == pitch.setValue(ax::minFrequency, "Nope"));
    REQUIRE(pitch.getFloatValue(ax::minFrequency) == Catch::Approx(1000.0));
}
TEST_CASE("SettingsRegistry_t", "[SettingsRegistry_t]") {
    using namespace analysis::modern;
    namespace ax = axiom::tsn;

    const AnalyzerSettingsRegistry_t registry;

    SECTION("get group by name") {
        std::optional<AnalyzerSettingsRegistry_t::ConstGroupVariant> NAgroupOpt = registry.getGroupVariant("nonexistent");
        REQUIRE(NAgroupOpt.has_value() == false);

        auto anGroupOpt = registry.getGroupVariant(ax::Analysis);
        REQUIRE(anGroupOpt.has_value());
        const auto groupVar = anGroupOpt.value();
        std::visit([&](auto& group) {
            const auto a = group.get().getIntValue(ax::frameSize);
            std::cout << a << "\n";
        }, groupVar);
    }
    SECTION("simpler API for value access") {
        const auto frameSize = registry.getInt(ax::Analysis, ax::frameSize).value();
        REQUIRE(frameSize == 1024);
        const auto pitchDetectionAlgo = registry.getString(ax::Pitch, ax::pitchDetectionAlgorithm).value();
        REQUIRE(pitchDetectionAlgo.toStdString() == ax::yin);
    }

    SECTION("simpler group access") {
        const auto frameSize = registry.getInt(ax::Analysis, ax::frameSize).value();

        // Option 1: Using the typed method (requires knowing the group type)
        if (auto analysisGroupOpt = registry.getGroupTyped<const AnalysisSettings>(ax::Analysis)) {
            auto& analysisGroup = analysisGroupOpt->get();
            const auto frameSizeFromGroup = analysisGroup.getIntValue(ax::frameSize);
            std::cout << "Frame size from group: " << frameSizeFromGroup << "\n";
        }

        // Option 2: Using compile-time group name (if you have string literal support)
        if constexpr (requires { registry.getGroupByName<ax::Analysis>(); }) {
            if (auto analysisGroupOpt = registry.getGroupByName<ax::Analysis>()) {
                REQUIRE(analysisGroupOpt.has_value());
                auto& analysisGroup = analysisGroupOpt->get();
                REQUIRE(std::string(analysisGroup.groupName) == ax::Analysis);
                const auto frameSizeFromGroup = analysisGroup.getIntValue(ax::frameSize);
                REQUIRE(frameSizeFromGroup == frameSize);
            }
        }
    }
}

TEST_CASE("Setting default value", "[RangedSetting]")
{
    using namespace analysis::modern;

    constexpr char name[] = "myFloat";
    RangedSetting<SInfo<name>, double, 0.5, -1.0, 2.0> s;

    // constexpr char *wontCompileName1 = "wontCompile";
    // analysis::modern::RangedSetting<SInfo<wontCompileName1>, double, 0.5, -1.0, 2.0> wontCompile1;

    // constexpr std::string_view wontCompileName2 {"wontCompile"};
    // analysis::modern::RangedSetting<SInfo<wontCompileName2>, double, 0.5, -1.0, 2.0> wontCompile2;

    REQUIRE(s.value == 0.5f);
    REQUIRE(s.defaultValue == 0.5f);
    REQUIRE(std::string(s.name) == "myFloat");
    REQUIRE(s.minValue == -1.0f);
    REQUIRE(s.maxValue == 2.0f);
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
        REQUIRE(std::get<double>(group.getValue("s1").value()) == Catch::Approx(0.1));
        REQUIRE(std::get<bool>(group.getValue("s2").value()) == true);
        REQUIRE(std::get<juce::String>(group.getValue("s3").value()) == "a");

        group.setValue("s1", 0.2);
        REQUIRE(std::get<double>(group.getValue("s1").value()) == Catch::Approx(0.2));
        group.setValue("s2", false);
        REQUIRE(std::get<bool>(group.getValue("s2").value()) == false);
        group.setValue("s3", "b");
        REQUIRE(std::get<juce::String>(group.getValue("s3").value()) == "b");

        const auto v = group.getFloatValue("s1");
        REQUIRE(v == Catch::Approx(0.2));
        REQUIRE(group.getStringValue("s3") == "b");

        REQUIRE(group.setValue("s2", 100.0) == false);
        REQUIRE(group.setValue("s1", 0.3) == true);
        REQUIRE(group.getFloatValue("s1") == Catch::Approx(0.3));
        REQUIRE(group.setValue("", true) == false);
        REQUIRE(group.setValue("s2", true) == true);
        REQUIRE(group.setValue("s1", "nope") == false);

#pragma message("Advanced: could require the following to actually fail, since \"yes\" is not one of the choices")
        REQUIRE(group.setValue("s3", "yes") == true);
        REQUIRE(group.getStringValue("s3") == "yes");
    }
    SECTION("resetDefaults") {
        Group group;
        const auto spec = group.getFloatSpec("s1");
        const auto defaultVal = spec.value().defaultValue;
        REQUIRE(group.getFloatValue("s1") == defaultVal);
        group.setValue("s1", 10.5);
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
        REQUIRE(fSpec.value().range.start == Catch::Approx(-1.0));
        REQUIRE(fSpec.value().range.end == Catch::Approx(2.0));

        auto bSpec = group.getBoolSpec("");
        REQUIRE(bSpec.has_value() == false);
        bSpec = group.getBoolSpec("s2");
        REQUIRE(bSpec.has_value());
        REQUIRE(bSpec.value().defaultValue == true);
    }
}

}   // namespace nvs::test