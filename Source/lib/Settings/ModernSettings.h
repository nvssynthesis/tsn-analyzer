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
using String = juce::String;
using ValueTree = juce::ValueTree;

// forward declarations for specs (to maintain compatibility with existing system)
template<typename T>
struct RangedSettingsSpec
{
    NormalisableRangeDouble range;
    T defaultValue;
    String tooltip = {};           // Optional tooltip
    String unit = {};              // e.g., "Hz", "dB", "ms"
    int numDecimalPlaces = 2;      // Default precision (unused if T is integral)
};
struct ChoiceSettingsSpec
{
    std::vector<String> options;
    String defaultValue;
    String tooltip = {};
};
struct BoolSettingsSpec
{
    bool defaultValue;
    String tooltip = {};
};

using AnySpec = std::variant<
    RangedSettingsSpec<int>,
    RangedSettingsSpec<double>,
    ChoiceSettingsSpec,
    BoolSettingsSpec
>;

// concepts
template<typename T>
concept SettingType = std::is_arithmetic_v<T> || std::is_same_v<T, String> || std::is_enum_v<T>;

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

template<typename /*SInfo*/Info, StringLiteral DefaultValue, StringLiteral... Choices>
struct ChoiceSetting {
    static_assert(sizeof...(Choices) >= 1, "ChoiceSetting must have at least one choice");

    using value_type = String;
    String value { DefaultValue.view().data() };
    static constexpr std::string_view name = Info::name;
    static constexpr std::string_view tooltip = Info::tooltip;
    static constexpr std::string_view defaultValue = DefaultValue.view();

    static auto createSpec() {
        return ChoiceSettingsSpec{{ String(Choices.view().data())... },
                                    String(DefaultValue.view().data()),
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
    static const std::map<String, AnySpec>& getSpecs() {
        static const auto specs = []() {
            std::map<String, AnySpec> map;
            auto addSpec = [&map]<typename S>(S*) {
                const AnySpec spec = S::createSpec();
                map[String(S::name.data())] = spec;
            };
            (addSpec(static_cast<Settings*>(nullptr)), ...);
            return map;
        }();
        return specs;
    }
    
private:
    template<typename Ret, typename Extractor>
    std::optional<Ret> getProperty(std::string_view name, Extractor extractor) const {
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
    std::optional<Spec> getSpec(std::string_view name) const {
        return getProperty<Spec>(name,
            [](const auto& s) {
                return s.createSpec();
            });
    }
    auto getBoolSpec(const std::string_view name)   const { return getSpec<BoolSettingsSpec>(name); }
    auto getFloatSpec(const std::string_view name)  const { return getSpec<RangedSettingsSpec<double>>(name); }
    auto getIntSpec(const std::string_view name)    const { return getSpec<RangedSettingsSpec<int>>(name); }
    auto getStringSpec(const std::string_view name) const { return getSpec<ChoiceSettingsSpec>(name); }

    using Value = std::variant<int, double, bool, String>;
    std::optional<Value> getValue(const std::string_view name) const {
        const auto valueExtractor = [](const auto& s) {
            return s.value;
        };
        return getProperty<Value>(std::string_view(name), valueExtractor);
    }
private:
    template<typename T>
    std::optional<T> getTypedOptional(const std::string_view name) const {
        const auto tmp = getValue(name);
        if (!tmp) {
            return std::nullopt;
        }
        if (std::holds_alternative<T>(*tmp)) {
            return std::get<T>(*tmp);
        }
        return std::nullopt;
    }
    template<typename T>
    T getTypedValue(const std::string_view name) const {
        const auto opt = getTypedOptional<T>(name);
        jassert(opt.has_value());   // catch runtime error in debug
        return opt.value();
    }
public:
    double getFloatValue(const std::string_view name)    const { return getTypedValue<double>(name); }
    int getIntValue(const std::string_view name)         const { return getTypedValue<int>(name); }
    String getStringValue(const std::string_view name)   const { return getTypedValue<String>(name); }
    bool getBoolValue(const std::string_view name)       const { return getTypedValue<bool>(name); }

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

    // auto-generate ValueTree serialization
    void toValueTree(ValueTree& parent) const {
        jassert(parent.isValid());
        auto child = parent.getOrCreateChildWithName(String(groupName.data()), nullptr);
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
            child.setProperty(String(name), value, nullptr);
        };
        std::apply([&serialize](const auto&... _settings) {
            (serialize(_settings), ...);
        }, settings);
    }
    
    // auto-generate ValueTree deserialization
    [[nodiscard]] ValueTree fromValueTree(const ValueTree& parent) {
        const auto s = String(groupName.data());
        const auto child = parent.getChildWithName(s);
        if (!child.isValid()) {
            // create missing child w/ default values
            DBG("fromValueTree: child invalid; creating default tree...");
            ValueTree defaultChild(s);

            // populate w/ default values
            auto populateDefault = [&defaultChild](auto& setting) {
                using SettingType = std::decay_t<decltype(setting)>;
                juce::var defaultValue;

                if constexpr (std::is_enum_v<typename SettingType::value_type>) {
                    defaultValue = static_cast<int>(SettingType::defaultValue);
                } else if constexpr (std::is_same_v<typename SettingType::value_type, String>) {
                    defaultValue = String(SettingType::defaultValue.data());
                } else {
                    defaultValue = SettingType::defaultValue;
                }

                defaultChild.setProperty(String(SettingType::name.data()), defaultValue, nullptr);
            };

            std::apply([&populateDefault](const auto&... _settings) {
                (populateDefault(_settings), ...);
            }, settings);

            return defaultChild;
        }

        // child exists but needs check for missing properties to fill in
        ValueTree correctedChild = child.createCopy();
        bool needsCorrection = false;

        auto checkAndCorrect = [&](auto& setting) {
            using SettingType = std::decay_t<decltype(setting)>;
            const auto propertyName = String(SettingType::name.data());

            if (!correctedChild.hasProperty(propertyName)) {
                needsCorrection = true;
                juce::var defaultValue;

                if constexpr (std::is_enum_v<typename SettingType::value_type>) {
                    defaultValue = static_cast<int>(SettingType::defaultValue);
                } else if constexpr (std::is_same_v<typename SettingType::value_type, String>) {
                    defaultValue = String(SettingType::defaultValue.data());
                } else {
                    defaultValue = SettingType::defaultValue;
                }

                correctedChild.setProperty(propertyName, defaultValue, nullptr);
            }
        };

        std::apply([&checkAndCorrect](const auto&... _settings) {
            (checkAndCorrect(_settings), ...);
        }, settings);

        // load existing values into our settings
        auto deserialize = [&correctedChild](auto& setting) {
            using SettingType = std::decay_t<decltype(setting)>;
            const auto propertyName = String(SettingType::name.data());

            if constexpr (std::is_enum_v<typename SettingType::value_type>) {
                setting.value = static_cast<typename SettingType::value_type>(
                    static_cast<int>(correctedChild.getProperty(propertyName)));
            } else {
                const juce::var val = correctedChild.getProperty(propertyName);
                setting.value = static_cast<decltype(setting.value)>(val);
            }
        };

        std::apply([&deserialize](auto&... _settings) {
            (deserialize(_settings), ...);
        }, settings);

        // return empty tree if no correction was needed, otherwise return the corrected tree
        return needsCorrection ? correctedChild : ValueTree();
    }
    
    void resetToDefaults() {
        auto reset = [](auto& setting) {
            using SettingType = std::decay_t<decltype(setting)>;
            using ValueType = typename SettingType::value_type;

            if constexpr (std::is_same_v<ValueType, String>) {
                // string will need conversion from string_view
                setting.value = String(SettingType::defaultValue.data());
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

    using GroupVariant = std::variant<std::reference_wrapper<Groups>...>;
    using ConstGroupVariant = std::variant<std::reference_wrapper<const Groups>...>;

    std::optional<GroupVariant> getGroupVariant(const std::string_view groupName) {
        std::optional<GroupVariant> ret;
        std::apply([&](auto&... s) {
            auto tryOne = [&](auto& group) {
                if (std::string_view(group.groupName) == groupName) {
                    ret = std::ref(group);
                }
            };
            (tryOne(s), ...);
        }, groups);
        return ret;
    }

    std::optional<ConstGroupVariant> getGroupVariant(const std::string_view groupName) const {
        std::optional<ConstGroupVariant> ret;
        std::apply([&](const auto&... s) {
            auto tryOne = [&](const auto& group) {
                if (std::string_view(group.groupName) == groupName) {
                    ret = std::cref(group);
                }
            };
            (tryOne(s), ...);
        }, groups);
        return ret;
    }

    template<StringLiteral GroupName>
    auto getGroupByName() const //-> std::optional<std::reference_wrapper<std::decay_t<decltype(std::get<findGroupIndex<GroupName>()>(groups))>>>
    {
        constexpr auto index = findGroupIndex<GroupName>();
        static_assert(index != sizeof...(Groups), "Group not found");

        using GroupType = std::decay_t<decltype(std::get<index>(groups))>;
        return std::optional<std::reference_wrapper<const GroupType>>{std::cref(std::get<index>(groups))};
    }

    template<typename GroupType>
    std::optional<std::reference_wrapper<const GroupType>> getGroupTyped(const std::string_view groupName) const {
        const auto groupOpt = getGroupVariant(groupName);
        if (!groupOpt.has_value()) {
            return std::nullopt;
        }
        const auto& groupVar = groupOpt.value();
        if (const auto* group = std::get_if<std::reference_wrapper<const GroupType>>(&groupVar)) {
            return *group;
        }
        return std::nullopt;
    }
    template<typename GroupType>
    std::optional<std::reference_wrapper<GroupType>> getGroupTyped(const std::string_view groupName) {
        auto groupOpt = getGroupVariant(groupName);
        if (!groupOpt.has_value()) {
            return std::nullopt;
        }
        auto& groupVar = groupOpt.value();
        if (auto* group = std::get_if<std::reference_wrapper<GroupType>>(&groupVar)) {
            return *group;
        }
        return std::nullopt;
    }
    // auto-generate specsByBranch
    static const std::map<String, const std::map<String, AnySpec>*>& getSpecsByBranch() {
        static const auto registry = []() {
            std::map<String, const std::map<String, AnySpec>*> map;
            auto addGroup = []<typename G>(G*, auto& _map) {
                _map[String(G::groupName.data())] = &G::getSpecs();
            };
            (addGroup(static_cast<Groups*>(nullptr), map), ...);
            return map;
        }();
        return registry;
    }

    // auto-generate createParentTreeFromSettings
    ValueTree createValueTree() const {
        ValueTree parent("Settings");
        std::apply([&parent](const auto&... _groups) {
            (_groups.toValueTree(parent), ...);
        }, groups);
        return parent;
    }

    // auto-generate updateSettingsFromValueTree
    [[nodiscard]] ValueTree fromValueTree(const ValueTree& tree) {
        ValueTree correctedTree = tree.createCopy();
        bool needsCorrection = false;

        std::apply([&](auto&... _groups) {
            auto processGroup = [&](auto& group) {
                if (const ValueTree groupCorrection = group.fromValueTree(correctedTree);   // core–what we do with groupCorrection is not integral to setting the SettingsGroup itself
                    groupCorrection.isValid())
                {
                    needsCorrection = true;
                    // Replace or add the corrected group
                    const String groupName(group.groupName.data());

                    // either replace (if exists but out of date) or freshly add (if nonexistent)
                    if (const auto existingChild = correctedTree.getChildWithName(groupName);
                        existingChild.isValid())
                    {
                        correctedTree.removeChild(existingChild, nullptr);
                    }
                    correctedTree.appendChild(groupCorrection, nullptr);
                }
            };

            (processGroup(_groups), ...);
        }, groups);

        return needsCorrection ? correctedTree : ValueTree();
    }

    // reset all groups to defaults
    void resetToDefaults() {
        std::apply([](auto&... _groups) {
            (_groups.resetToDefaults(), ...);
        }, groups);
    }
    template<typename T>
    bool setSetting(const std::string_view groupName, const std::string_view settingName, const T& value) {
        auto groupOpt = getGroupVariant(groupName);
        if (!groupOpt) return false;

        bool result = false;
        std::visit([&](auto& groupRef) {
            auto& group = groupRef.get();
            result = group.setValue(settingName, value);
        }, *groupOpt);
        return result;
    }

    std::optional<int> getInt(const std::string_view groupName, const std::string_view settingName) const {
        return getSetting<int>(groupName, settingName);
    }

    std::optional<double> getFloat(const std::string_view groupName, const std::string_view settingName) const {
        return getSetting<double>(groupName, settingName);
    }

    std::optional<bool> getBool(const std::string_view groupName, const std::string_view settingName) const {
        return getSetting<bool>(groupName, settingName);
    }

    std::optional<String> getString(const std::string_view groupName, const std::string_view settingName) const {
        return getSetting<String>(groupName, settingName);
    }
private:
    template<StringLiteral GroupName>
    static constexpr std::size_t findGroupIndex() {
        std::size_t index = 0;
        std::size_t result = sizeof...(Groups); // default to "not found"

        // iterate through groups and find matching name
        ((std::string_view(Groups::groupName) == GroupName.view() ?
            (result = index, false) :   // always evaluating the condition to false prevents ever short circuiting (assign to result if match)
            (++index, false)) || ...);

        return result;
    }
    template<typename T>
    std::optional<T> getSetting(const std::string_view groupName, const std::string_view settingName) const {
        auto groupOpt = getGroupVariant(groupName);
        if (!groupOpt) return std::nullopt;

        std::optional<T> result;
        std::visit([&](auto& groupRef) {
            auto& group = groupRef.get();
            if constexpr (std::is_same_v<T, int>) {
                result = group.getIntValue(settingName);
            } else if constexpr (std::is_same_v<T, double>) {
                result = group.getFloatValue(settingName);
            } else if constexpr (std::is_same_v<T, bool>) {
                result = group.getBoolValue(settingName);
            } else if constexpr (std::is_same_v<T, String>) {
                result = group.getStringValue(settingName);
            }
        }, *groupOpt);
        return result;
    }

};

} // namespace nvs::analysis::modern
