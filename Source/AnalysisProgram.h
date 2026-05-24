#pragma once

#include <JuceHeader.h>
#include <span>
#include "lib/ThreadedAnalyzer.h"
#include "lib/TSNValueTreeUtilities.h"
#include "lib/Settings/Settings.h"

using namespace juce;

struct AudioFileInfo {
    int64 numSamples;
    double sampleRate;
    unsigned int bitDepth;
};

struct AnalyzerResult {
    std::shared_ptr<nvs::analysis::TimbreAnalysisResult> timbres {};
    std::shared_ptr<nvs::analysis::OnsetAnalysisResult> onsets {};
    std::shared_ptr<nvs::analysis::PacmapResult> pacmap {};
    String settingsHash;
};

void mainAnalysisProgram(const ArgumentList &args);
