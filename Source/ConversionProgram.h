//
// Created by Nicholas Solem on 2/3/26.
//

#pragma once
#include "juce_utils.h"
#include "ProgramUtils.h"

inline void conversionProgram(const ArgumentList &args) {
    if (args.arguments.size() < 2) {
        Logger::writeToLog("Not enough arguments");
        return;
    }
    const auto inFile = getInputFile(args);
    const auto inFileExt = inFile.getFileExtension();

    const auto outFile = [&args, &inFile, &inFileExt]() -> File {
        if (args.arguments.size() < 3) {
            if (inFileExt == ".tsb") {
                return inFile.withFileExtension(".json");
            }
            if (inFileExt == ".json") {
                return inFile.withFileExtension(".tsb");
            }
            Logger::writeToLog("extensions must be either .json or .tsb");
            return {};
        }
        return getOutputFile(args);
    }();
    if (outFile == File()) {
        Logger::writeToLog("invalid extension; returning...");
        return;
    }
    const auto outFileExt = outFile.getFileExtension();

    if ( (inFileExt != ".tsb" && inFileExt != ".json") || (outFileExt != ".tsb" && outFileExt != ".json") )
    {
        Logger::writeToLog("extensions must be either .json or .tsb");
        return;
    }
    if (inFileExt == outFileExt) {
        Logger::writeToLog("extensions must be complimentary (e.g. if input extension is .tsb, output should be .json)");
        return;
    }

    if (inFileExt == ".tsb") {
        if (const ValueTree analysisVT = nvs::util::loadValueTreeFromBinary(inFile);
            nvs::analysis::validateAnalysisVT(analysisVT))
        {
            nvs::util::saveValueTreeToJSON(analysisVT, outFile);
        }
    }
    else {
        if (const ValueTree analysisVT = nvs::util::loadValueTreeFromJSON(inFile);
            nvs::analysis::validateAnalysisVT(analysisVT))
        {
            nvs::util::saveValueTreeToBinary(analysisVT, outFile);
        }
    }
}