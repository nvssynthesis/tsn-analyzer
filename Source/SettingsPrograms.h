#pragma once
#include "SettingsPresets.h"

using juce::ValueTree;
using juce::File;
using juce::ArgumentList;


void printSettingsPath(const ArgumentList &);

void printCurrentSettings(const ArgumentList &);

void createSettingsPresetFromDefault(const ArgumentList &args);

// private utilities
void printValueTreeFile(const File &valueTreeFile);
ValueTree createDefaultPresetFile();