/*
==============================================================================

    ModernSettings.h
    Created: 21 May 2026 12:19:09am
    Author:  Nicholas Solem

  ==============================================================================
*/

#pragma once
#include <concepts>
#include <string_view>
#include <tuple>
#include <map>
#include <variant>
#include <type_traits>
#include <juce_data_structures/juce_data_structures.h>

namespace nvs::analysis::modern {

using NormalisableRangeDouble = juce::NormalisableRange<double>;

// forward declarations for specs (to maintain compatibility with existing system)
template<typename T>
struct RangedSettingsSpec
{
    NormalisableRangeDouble range;
    T defaultValue;
    juce::String tooltip = {};           // Optional tooltip
    int numDecimalPlaces = 2;            // Default precision
    juce::String unit = {};              // e.g., "Hz", "dB", "ms"
};
struct ChoiceSettingsSpec
{
    std::vector<juce::String> options;
    juce::String defaultValue;
    juce::String tooltip = {};
};
struct BoolSettingsSpec
{
    bool defaultValue;
    juce::String tooltip = {};
};

using AnySpec = std::variant<RangedSettingsSpec<int>, RangedSettingsSpec<double>, ChoiceSettingsSpec, BoolSettingsSpec>;

// concepts
template<typename T>
concept SettingType = std::is_arithmetic_v<T> || std::is_same_v<T, juce::String> || std::is_enum_v<T>;

template<typename T>
concept NumericSettingType = std::is_arithmetic_v<T> && !std::is_same_v<T, bool>;

struct NumericParam {
    double value;
    constexpr NumericParam(const double v) : value(v) {} // NOLINT(google-explicit-constructor)
    constexpr NumericParam(const float v) : value(v) {} // NOLINT(google-explicit-constructor)
    constexpr NumericParam(const int v) : value(static_cast<double>(v)) {} // NOLINT(google-explicit-constructor)
};

// string literal wrappers for choices
template<std::size_t N>
struct StringLiteral {
    constexpr StringLiteral(const char (&str)[N]) { // NOLINT(google-explicit-constructor)
        std::copy_n(str, N, value);
    }
    constexpr std::string_view view() const { return {value, N - 1}; }
    char value[N]{};
};

// setting template with compile-time metadata
template<bool DefaultValue, StringLiteral Name>
struct BoolSetting {
    using value_type = bool;
    bool value = DefaultValue;
    static constexpr std::string_view name = Name.view();
    static constexpr bool defaultValue = DefaultValue;
    
    static constexpr auto createSpec() {
        return BoolSettingsSpec{DefaultValue};
    }
};

// specialized settings with additional metadata
template<NumericSettingType T, NumericParam DefaultValue, StringLiteral Name, NumericParam Min, NumericParam Max>
struct RangedSetting {
    using value_type = T;
    T value = static_cast<T>(DefaultValue.value);
    static constexpr std::string_view name = Name.view();
    static constexpr T defaultValue = static_cast<T>(DefaultValue.value);
    static constexpr T minValue = static_cast<T>(Min.value);
    static constexpr T maxValue = static_cast<T>(Max.value);

    static auto createSpec() {
        juce::NormalisableRange<double> range{Min.value, Max.value};
        return RangedSettingsSpec<T>{range, static_cast<T>(DefaultValue.value)};
    }
};

template<StringLiteral DefaultValue, StringLiteral Name, StringLiteral... Choices>
struct ChoiceSetting {
    using value_type = juce::String;
    juce::String value { DefaultValue.view().data() };
    static constexpr std::string_view name = Name.view();
    static constexpr std::string_view defaultValue = DefaultValue.view();

    static auto createSpec() {
        return ChoiceSettingsSpec{{ juce::String(Choices.view().data())... },
                                    juce::String(DefaultValue.view().data())};
    }
};

// settings group w/ auto-generation capabilities
template<StringLiteral GroupName, typename... Settings>
struct SettingsGroup {
    std::tuple<Settings...> settings;
    static constexpr std::string_view groupName = GroupName.view();
    static constexpr size_t numSettings = sizeof...(Settings);
    
    // auto-generate specs map
    static const std::map<juce::String, AnySpec>& getSpecs() {
        static const auto specs = []() {
            std::map<juce::String, AnySpec> map;
            auto addSpec = [&map]<typename S>(S*) {
                const AnySpec spec = S::createSpec();
                map[juce::String(S::name.data())] = spec;
            };
            (addSpec(static_cast<Settings*>(nullptr)), ...);
            return map;
        }();
        return specs;
    }
    
    // get setting by index
    template<size_t I>
    auto& get() { return std::get<I>(settings); }
    
    template<size_t I>
    const auto& get() const { return std::get<I>(settings); }
    
    // auto-generate ValueTree serialization
    void toValueTree(juce::ValueTree& parent) const {
        jassert(parent.isValid());
        auto child = parent.getOrCreateChildWithName(juce::String(groupName.data()), nullptr);
        auto serialize = [&child](const auto& setting) {
            using SettingType = std::decay_t<decltype(setting)>;
            juce::var value;
            if constexpr (std::is_enum_v<typename SettingType::value_type>) {
                // enums are stored as ints
#pragma message("enable storing enums as strings...")
                value = static_cast<int>(setting.value);
            } else {
                value = setting.value;
            }
            const auto name = SettingType::name.data();
            child.setProperty(juce::String(name), value, nullptr);
        };
        std::apply([&serialize](const auto&... _settings) {
            (serialize(_settings), ...);
        }, settings);
    }
    
    // auto-generate ValueTree deserialization
    void fromValueTree(const juce::ValueTree& parent) {
        auto child = parent.getChildWithName(juce::String(groupName.data()));
        if (!child.isValid()) return;
        
        auto deserialize = [&child](auto& setting) {
            using SettingType = std::decay_t<decltype(setting)>;
            const auto propertyName = juce::String(SettingType::name.data());
            if (child.hasProperty(propertyName)) {
                if constexpr (std::is_enum_v<typename SettingType::value_type>) {
                    setting.value = static_cast<typename SettingType::value_type>(
                        static_cast<int>(child.getProperty(propertyName)));
                } else {
                    const juce::var val = child.getProperty(propertyName);
                    setting.value = static_cast<decltype(setting.value)>(val);
                }
            }
        };
        std::apply([&deserialize](auto&... _settings) {
            (deserialize(_settings), ...);
        }, settings);
    }
    
    void resetToDefaults() {
        auto reset = [](auto& setting) {
            setting.value = std::decay_t<decltype(setting)>::defaultValue;
        };
        std::apply([&reset](auto&... _settings) {
            (reset(_settings), ...);
        }, settings);
    }
};

// settings registry - manages all settings groups
template<typename... Groups>
struct SettingsRegistry {
    std::tuple<Groups...> groups;
    static constexpr size_t numGroups = sizeof...(Groups);
    
    // get group by index
    template<size_t I>
    auto& get() { return std::get<I>(groups); }
    
    template<size_t I>
    const auto& get() const { return std::get<I>(groups); }
    
    // auto-generate specsByBranch
    static const std::map<juce::String, const std::map<juce::String,AnySpec>*>& getSpecsByBranch() {
        static const auto registry = []() {
            std::map<juce::String, const std::map<juce::String,AnySpec>*> map;
            auto addGroup = []<typename G>(G*, auto& _map) {
                _map[juce::String(G::groupName.data())] = &G::getSpecs();
            };
            (addGroup(static_cast<Groups*>(nullptr), map), ...);
            return map;
        }();
        return registry;
    }
    
    // auto-generate createParentTreeFromSettings
    juce::ValueTree createValueTree() const {
        juce::ValueTree parent("Settings");
        std::apply([&parent](const auto&... _groups) {
            (_groups.toValueTree(parent), ...);
        }, groups);
        return parent;
    }
    
    // auto-generate updateSettingsFromValueTree
    void fromValueTree(const juce::ValueTree& tree) {
        std::apply([&tree](auto&... _groups) {
            (_groups.fromValueTree(tree), ...);
        }, groups);
    }
    
    // reset all groups to defaults
    void resetToDefaults() {
        std::apply([](auto&... _groups) {
            (_groups.resetToDefaults(), ...);
        }, groups);
    }
};

} // namespace nvs::analysis::modern
