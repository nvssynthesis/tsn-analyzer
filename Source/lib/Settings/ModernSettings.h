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

#include "fmt/base.h"

namespace nvs::analysis::modern {

using NormalisableRangeDouble = juce::NormalisableRange<double>;

// forward declarations for specs (to maintain compatibility with existing system)
template<typename T>
struct RangedSettingsSpec
{
    NormalisableRangeDouble range;
    T defaultValue;
    juce::String tooltip = {};           // Optional tooltip
    juce::String unit = {};              // e.g., "Hz", "dB", "ms"
    int numDecimalPlaces = 2;            // Default precision (unused if T is integral)
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

using AnySpec = std::variant<
    RangedSettingsSpec<int>,
    RangedSettingsSpec<double>,
    ChoiceSettingsSpec,
    BoolSettingsSpec
>;

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

template<auto N, auto end, typename F>
constexpr void constexpr_for(F&& f) {
    if constexpr (N < end) {
        f(std::integral_constant<decltype(N), N>{});
        constexpr_for<N + 1, end>(std::forward<F>(f));
    }
}

template<StringLiteral Name, StringLiteral Tooltip="">
struct SInfo {
    // this struct packs common string info into one effective template parameter.
    // very useful because Tooltip should be optional, but this came into conflict with ChoiceSettings
    // needing a parameter pack yet also taking an optional tooltip parameter.
    static constexpr std::string_view name = Name.view();
    static constexpr std::string_view tooltip = Tooltip.view();
};

// setting template with compile-time metadata
template<typename /*SInfo*/Info, bool DefaultValue>
struct BoolSetting {
    using value_type = bool;
    bool value = DefaultValue;
    static constexpr std::string_view name = Info::name;
    static constexpr std::string_view tooltip = Info::tooltip;
    static constexpr bool defaultValue = DefaultValue;

    static constexpr auto createSpec() {
        return BoolSettingsSpec{DefaultValue, Info::tooltip.data()};
    }
};

// specialized settings with additional metadata
template<typename /*SInfo*/Info, NumericSettingType T, NumericParam DefaultValue,
    NumericParam Min, NumericParam Max,
    StringLiteral Unit="">
struct RangedSetting {
    using value_type = T;
    T value = static_cast<T>(DefaultValue.value);
    static constexpr std::string_view name = Info::name;
    static constexpr std::string_view tooltip = Info::tooltip;
    static constexpr T defaultValue = static_cast<T>(DefaultValue.value);
    static constexpr T minValue = static_cast<T>(Min.value);
    static constexpr T maxValue = static_cast<T>(Max.value);
    static constexpr std::string_view unit = Unit.view();

    static auto createSpec() {
        juce::NormalisableRange<double> range{Min.value, Max.value};
        return RangedSettingsSpec<T>{range,
            static_cast<T>(DefaultValue.value),
            Info::tooltip.data(), Unit.view().data()};
    }
};

template<typename /*SInfo*/ Info, StringLiteral DefaultValue, StringLiteral... Choices>
struct ChoiceSetting {
    static_assert(sizeof...(Choices) >= 1, "ChoiceSetting must have at least one choice");

    using value_type = juce::String;
    juce::String value { DefaultValue.view().data() };
    static constexpr std::string_view name = Info::name;
    static constexpr std::string_view tooltip = Info::tooltip;
    static constexpr std::string_view defaultValue = DefaultValue.view();

    static auto createSpec() {
        return ChoiceSettingsSpec{{ juce::String(Choices.view().data())... },
                                    juce::String(DefaultValue.view().data()),
                                    Info::tooltip.data()};
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
    
private:
    template<typename Ret, typename Extractor>
    std::optional<Ret> getProperty(std::string_view name, Extractor extractor) {
        std::optional<Ret> ret;
        std::apply([&](auto&... s) {
            auto tryOne = [&](auto& setting) {
                if (setting.name == name) {
                    // only try to extract if the types are compatible
                    if constexpr (std::is_convertible_v<decltype(extractor(setting)), Ret>) {
                        ret = extractor(setting);
                    }
                }
            };
            (tryOne(s), ...);
        }, settings);
        return ret;
    }
public:
    template<typename Spec>
    std::optional<Spec> getSpec(std::string_view name) {
        return getProperty<Spec>(name,
            [](const auto& s) {
                return s.createSpec();
            });
    }
    auto getBoolSpec(const std::string_view name)   { return getSpec<BoolSettingsSpec>(name); }
    auto getFloatSpec(const std::string_view name)  { return getSpec<RangedSettingsSpec<double>>(name); }
    auto getIntSpec(const std::string_view name)    { return getSpec<RangedSettingsSpec<int>>(name); }
    auto getStringSpec(const std::string_view name) { return getSpec<ChoiceSettingsSpec>(name); }

    using Value = std::variant<int, double, bool, juce::String>;
    std::optional<Value> getValue(const std::string_view name) {
        const auto valueExtractor = [](const auto& s) {
            return s.value;
        };
        return getProperty<Value>(std::string_view(name), valueExtractor);
    }
private:
    template<typename T>
    std::optional<T> getTypedValue(const std::string_view name) {
        const auto tmp = getValue(name);
        if (!tmp) {
            return std::nullopt;
        }
        if (std::holds_alternative<T>(*tmp)) {
            return std::get<T>(*tmp);
        }
        return std::nullopt;
    }
public:
    std::optional<double> getFloatValue(const std::string_view name) { return getTypedValue<double>(name); }
    std::optional<juce::String> getStringValue(const std::string_view name) { return getTypedValue<juce::String>(name); }
    std::optional<bool> getBoolValue(const std::string_view name) { return getTypedValue<bool>(name); }

    bool setValue(const std::string_view name, const Value& newVal) {
        auto _setValue = [&](auto& setting) {
            if (setting.name == name) {
                using SettingValueType = std::decay_t<decltype(setting.value)>;
                if constexpr (requires { std::get<SettingValueType>(newVal); }) {
                    if (std::holds_alternative<SettingValueType>(newVal)) {
                        setting.value = std::get<SettingValueType>(newVal);
                        return true;
                    }
                }
            }
            return false;
        };
        return std::apply([&_setValue](auto&... s) {
            return (_setValue(s) || ...);
        }, settings);
    }
    bool setBoolValue(const std::string_view name, const bool newVal) { return setValue(name, newVal); }
    bool setFloatValue(const std::string_view name, const double newVal) { return setValue(name, newVal); }
    bool setStringValue(const std::string_view name, const juce::String& newVal) { return setValue(name, newVal); }

    // auto-generate ValueTree serialization
    void toValueTree(juce::ValueTree& parent) const {
        jassert(parent.isValid());
        auto child = parent.getOrCreateChildWithName(juce::String(groupName.data()), nullptr);
        auto serialize = [&child]<typename T0>(const T0& setting) {
            using SettingType = std::decay_t<T0>;
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
        if (!child.isValid()) {
            DBG("fromValueTree: child invalid; returning...");
            return;
        }
        
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
            using SettingType = std::decay_t<decltype(setting)>;
            using ValueType = typename SettingType::value_type;

            if constexpr (std::is_same_v<ValueType, juce::String>) {
                // juce::String will need conversion from string_view
                setting.value = juce::String(SettingType::defaultValue.data());
            } else {
                setting.value = SettingType::defaultValue;
            }
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
