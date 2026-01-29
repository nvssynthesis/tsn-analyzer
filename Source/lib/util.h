//
// Created by Nicholas Solem on 1/28/26.
//

#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <juce_data_structures/juce_data_structures.h>

namespace nvs::util {

inline juce::String hashAudioData(const std::vector<float>& audioData) {
    const auto hash = juce::SHA256(audioData.data(), audioData.size() * sizeof(float));
    return hash.toHexString();
}

template < typename C, C beginVal, C endVal>
class Iterator {
    typedef typename std::underlying_type<C>::type val_t;
    int val;
public:
    explicit Iterator(const C & f) : val(static_cast<val_t>(f)) {}
    Iterator() : val(static_cast<val_t>(beginVal)) {}
    Iterator operator++() {
        ++val;
        return *this;
    }
    C operator*() { return static_cast<C>(val); }
    Iterator begin() { return *this; } //default ctor is good
    Iterator end() {
        static const Iterator endIter=++Iterator(endVal); // cache it
        return endIter;
    }
    bool operator!=(const Iterator& i) { return val != i.val; }
};

inline juce::String hashValueTree(const juce::ValueTree& settings)
{
    auto xmlElement = settings.createXml();
    if (!xmlElement) {
        std::cerr << "hashValueTree: !xmlElement\n";
        jassertfalse;
        return juce::String();
    }
    juce::String xmlString = xmlElement->toString();

    auto hash = juce::SHA256(xmlString.toUTF8(), xmlString.getNumBytesAsUTF8());
    return hash.toHexString();
}


juce::String getAndMigrateAudioHash(juce::ValueTree& metadataTree);

juce::String valueTreeToXmlStringSafe(const juce::ValueTree& tree);

inline bool isEmpty(juce::ValueTree const &vt){
    return (vt.getNumChildren() == 0 && vt.getNumProperties() == 0);
}
inline void clear(juce::ValueTree &vt){
    // Remove all children
    while (vt.getNumChildren() > 0) {
        vt.removeChild(0, nullptr);
    }

    // Remove all properties
    for (int i = vt.getNumProperties() - 1; i >= 0; --i) {
        vt.removeProperty(vt.getPropertyName(i), nullptr);
    }
}


inline juce::var valueTreeToVar(const juce::ValueTree& tree) {
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    // Add type identifier
    obj->setProperty("type", tree.getType().toString());

    // Add all properties
    for (int i = 0; i < tree.getNumProperties(); ++i) {
        auto name = tree.getPropertyName(i);
        auto value = tree.getProperty(name);
        obj->setProperty(name, value);
    }

    // Add children
    if (tree.getNumChildren() > 0) {
        juce::Array<juce::var> children;
        for (auto child : tree) {
            children.add(valueTreeToVar(child));
        }
        obj->setProperty("children", children);
    }

    return juce::var(obj.get());
}

inline bool saveValueTreeToBinary(const juce::ValueTree& tree, const juce::File& file) {
    if (juce::FileOutputStream stream(file); stream.openedOk())
    {
        tree.writeToStream(stream);
        return stream.getStatus().wasOk();
    }
    return false;
}
inline juce::ValueTree loadValueTreeFromBinary(const juce::File& file) {
    if (juce::FileInputStream stream(file);
        stream.openedOk())
    {
        return juce::ValueTree::readFromStream(stream);
    }
    return juce::ValueTree();
}
inline bool saveValueTreeToJSON(const juce::ValueTree& tree, const juce::File& file) {
    const juce::var jsonData = valueTreeToVar(tree);
    const juce::String jsonString = juce::JSON::toString(jsonData);

    return file.replaceWithText(jsonString);
}

} // namespace nvs::util