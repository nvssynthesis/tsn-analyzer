#include "util.h"
#include "StringAxiom.h"

namespace nvs::util {

juce::String getAndMigrateAudioHash(juce::ValueTree& metadataTree) {
    // a utility function to match against the hash coming from the key of older versions of the value tree
    auto hash = metadataTree.getProperty(nvs::axiom::audioHash).toString();
    if (hash.isEmpty()) {
        hash = metadataTree.getProperty("waveformHash").toString();
        if (hash.isNotEmpty()) {
            // Migrate old to new
            metadataTree.setProperty(nvs::axiom::audioHash, hash, nullptr);
            metadataTree.removeProperty("waveformHash", nullptr);
        }
    }
    if (hash.isEmpty()) {
        hash = metadataTree.getProperty("AudioFileHash").toString();
        if (hash.isNotEmpty()) {
            // Migrate old to new
            metadataTree.setProperty(nvs::axiom::audioHash, hash, nullptr);
            metadataTree.removeProperty("AudioFileHash", nullptr);
        }
    }
    return hash;
}

juce::String sanitizeXmlName(const juce::String& name)
{
    // Replace invalid characters with underscores
    juce::String sanitized;

    for (int i = 0; i < name.length(); ++i)
    {
        auto c = name[i];

        // First character must be letter or underscore
        if (i == 0)
        {
            if (juce::CharacterFunctions::isLetter(c) || c == '_')
                sanitized += c;
            else
                sanitized += '_';
        }
        else
        {
            // Subsequent chars can be letter, digit, underscore, hyphen, or period
            if (juce::CharacterFunctions::isLetterOrDigit(c) || c == '_' || c == '-' || c == '.')
                sanitized += c;
            else
                sanitized += '_';
        }
    }

    // Ensure we have at least something
    if (sanitized.isEmpty())
        sanitized = "_";

    return sanitized;
}

juce::String valueTreeToXmlStringSafe(const juce::ValueTree& tree)
{
    // Create a deep copy to avoid modifying the original
    juce::ValueTree safeCopy = tree.createCopy();

    // Lambda to recursively process the tree
    std::function<void(juce::ValueTree&)> processTree = [&](juce::ValueTree& vt)
    {
        // Collect properties to rename/replace
        juce::Array<juce::Identifier> propsToRemove;
        juce::Array<std::pair<juce::Identifier, juce::var>> propsToAdd;

        // Process all properties
        for (int i = 0; i < vt.getNumProperties(); ++i)
        {
            auto propName = vt.getPropertyName(i);
            auto value = vt.getProperty(propName);
            juce::String propNameStr = propName.toString();

            // Check if property name needs sanitization
            juce::String sanitizedName = sanitizeXmlName(propNameStr);
            bool nameNeedsSanitizing = (sanitizedName != propNameStr);

            // Check if it's an array
            if (auto* arr = value.getArray())
            {
                // Replace array with descriptive string
                juce::String arrayDesc = "array of length " + juce::String(arr->size());

                // Optionally include type info if array is not empty
                if (arr->size() > 0)
                {
                    auto firstElement = (*arr)[0];
                    if (firstElement.isInt())
                        arrayDesc += " (int)";
                    else if (firstElement.isDouble())
                        arrayDesc += " (double)";
                    else if (firstElement.isString())
                        arrayDesc += " (string)";
                    else if (firstElement.isBool())
                        arrayDesc += " (bool)";
                    else
                        arrayDesc += " (mixed/other)";
                }

                if (nameNeedsSanitizing)
                {
                    propsToRemove.add(propName);
                    propsToAdd.add({juce::Identifier(sanitizedName), arrayDesc});
                }
                else
                {
                    vt.setProperty(propName, arrayDesc, nullptr);
                }
            }
            else if (nameNeedsSanitizing)
            {
                // Property name needs sanitizing but value is fine
                propsToRemove.add(propName);
                propsToAdd.add({juce::Identifier(sanitizedName), value});
            }
        }

        // Apply property name changes
        for (auto& prop : propsToRemove)
            vt.removeProperty(prop, nullptr);

        for (auto& [id, val] : propsToAdd)
            vt.setProperty(id, val, nullptr);

        // Recursively process all children
        for (auto child : vt)
            processTree(child);
    };

    processTree(safeCopy);

    return safeCopy.toXmlString();
}

} // nnamespace nvs::util