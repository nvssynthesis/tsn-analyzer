//
// Created by Nicholas Solem on 6/4/26.
//

#include <catch2/catch_all.hpp>

#include "juce_utils.h"
#include "../cmake-build-debug/_deps/catch2-src/src/catch2/catch_test_macros.hpp"
#include "Settings/ModernSettings.h"
#include "Settings/ModernSettingsTypes.h"

namespace nvs::test {
using namespace analysis::modern;
namespace ax = axiom::tsn;
using juce::ValueTree;
using juce::String;

TEST_CASE("OverwriteSettings", "[SettingsRegistry]") {
    using B1 = BoolSetting<SInfo<"B1">, true>;
    using B2 = BoolSetting<SInfo<"B2">, false>;
    using G1 = SettingsGroup<"G1", B1, B2>;
    using R1 = SettingsRegistry<G1>;

    SECTION("Completely unfilled") {
        ValueTree registryVT("Registry");

        R1 r1;

        auto const regVToverwrite = r1.fromValueTree(registryVT);
        std::cout << "output tree: \n" << util::valueTreeToXmlStringSafe(regVToverwrite);

        const auto g1VT = regVToverwrite.getChildWithName("G1");
        REQUIRE(g1VT.isValid());
        REQUIRE(g1VT.hasProperty("B1"));
        REQUIRE(g1VT.hasProperty("B2"));
    }
    SECTION("Partially filled") {
        ValueTree registryVT("Registry");
        ValueTree g1VT("G1");
        g1VT.setProperty("B1", true, nullptr);
        registryVT.appendChild(g1VT, nullptr);

        R1 r1;
        const auto updatedVT = r1.fromValueTree(registryVT);
        std::cout << "output tree: \n" << util::valueTreeToXmlStringSafe(updatedVT);

        REQUIRE(updatedVT.isValid());
        REQUIRE(updatedVT.getChildWithName("G1").hasProperty("B1"));
        REQUIRE(updatedVT.getChildWithName("G1").hasProperty("B2"));
        REQUIRE(!g1VT.isEquivalentTo(updatedVT));
    }
    SECTION("Already filled") {

    }
}

}