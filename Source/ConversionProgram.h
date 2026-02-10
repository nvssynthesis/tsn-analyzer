//
// Created by Nicholas Solem on 2/3/26.
//

#pragma once
#include "juce_utils.h"
#include "ProgramUtils.h"
#include "./lib/config.h"

inline void conversionProgram(const ArgumentList &args) {
    if (args.arguments.size() < 2) {
        Logger::writeToLog("Not enough arguments");
        return;
    }
    const auto inFile = getInputFile(args, File::getCurrentWorkingDirectory());
    if (!inFile.existsAsFile()) {
        Logger::writeToLog("File " + inFile.getFullPathName() + " does not exist; returning...");
        return;
    }

    const auto inFileExt = inFile.getFileExtension();
    const auto outFile = [&args, &inFile, &inFileExt]() -> File {
        // try making outFile from inFile if only args are program name and program mode
        if (inFileExt == ".tsb") {
            return inFile.withFileExtension(".json");
        }
        if (inFileExt == ".json") {
            return inFile.withFileExtension(".tsb");
        }
        Logger::writeToLog("extensions must be either .json or .tsb");
        return {};
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

    if (outFile.existsAsFile() && (!args.containsOption("--force|-f"))) {
        std::cout << "Output file with path " << outFile.getFullPathName() << " already exists. Overwrite?\n";
        if (!checkForYesNoResponse()) {
            std::cout << "File not written; returning..." << std::endl;
            return;
        }
    }

    std::cout << "Converting " << inFile.getFullPathName() << " to \n" << outFile.getFullPathName() << std::endl;
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