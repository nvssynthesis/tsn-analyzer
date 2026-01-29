//
// Created by Nicholas Solem on 1/28/26.
//

#pragma once
#include <juce_cryptography/juce_cryptography.h>

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

} // namespace nvs::util