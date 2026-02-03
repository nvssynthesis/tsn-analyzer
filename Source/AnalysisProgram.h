#pragma once

#include <JuceHeader.h>
#include <span>
#include "lib/ThreadedAnalyzer.h"
#include "lib/TSNValueTreeUtilities.h"
#include "lib/Settings.h"

using namespace juce;

struct AudioFileInfo {
    int64 numSamples;
    double sampleRate;
    unsigned int bitDepth;
};

struct AnalyzerResult {
    std::optional<nvs::analysis::TimbreAnalysisResult> timbres {};
    std::shared_ptr<nvs::analysis::OnsetAnalysisResult> onsets {};
    String settingsHash;
};

AudioFileInfo readIntoBuffer(AudioSampleBuffer &buff, const File &file);
ValueTree makeSettingsParentTree(double sampleRate, const String &filePath);
AnalyzerResult runAnalyzer(const std::span<const float> &channel, const String &audioFileFullAbsolutePath, auto &settingsTree);
void mainAnalysisProgram(const ArgumentList &args);
