//
// Created by Nicholas Solem on 2/3/26.
//

#pragma once
#include <JuceHeader.h>

inline bool createParentDirectories(const File &f) {
    if (const Result res = f.getParentDirectory().createDirectory();
        !res)
    {
        Logger::writeToLog("In getOutputFile, error when creating directories: ");
        Logger::writeToLog(res.getErrorMessage() + "\n returning...");
        return false;
    }
    return true;
}

inline File asAbsPathOrWithinDirectory(const StringRef fileEntry, const File &directoryWithin) {
    if (File::isAbsolutePath(fileEntry)) {  // if they went out of their way to specify a full path, they can have it
        return File(fileEntry);
    }
    return directoryWithin.getChildFile(fileEntry); // otherwise we work from parent directoryWithin
}

inline File getInputFile(const ArgumentList &args, const File &directoryWithin, const bool shouldCreateParentDirectories=true)
{
    // of course this only applies for modes where 1st arg is input file.
    const String inputFileEntry = args.getValueForOption("--input|-i");
    if (inputFileEntry.isEmpty()) {
        return {};
    }
    const auto inputFile = asAbsPathOrWithinDirectory(inputFileEntry, directoryWithin);
    if (shouldCreateParentDirectories) {
        createParentDirectories(inputFile);
    }
    if (!inputFile.exists()) {
        return {};
    }
    return inputFile;
}

inline bool foundFlag(const ArgumentList &args, const Array<String> &flagsToFind) {
    // NOTE: pass in the RAW ARGS. this will look for the flag STARTING FROM 1 after the beginning, so as to exclude the program mode.
    for (auto const &flag : flagsToFind) {
        if (std::ranges::find(
        std::next(args.arguments.begin()),
        args.arguments.end(),
        flag, [](const auto &x) {
            return x.text;
        }) != args.arguments.end()) {
            return true;
        }
    }
    return false;
}

inline bool checkForYesNoResponse() {
    std::string response;
    auto checkResponse = [&response]() {
        if (!(String(response).startsWithIgnoreCase("y") || String(response).startsWithIgnoreCase("n"))) {
            std::cout << "please answer with 'y' or 'N' (case insensitive)" << std::endl;
            return false;
        }
        return true;
    };
    do {
        std::cin >> response;
    } while (!checkResponse());
    const auto jresponse = String(response).toLowerCase();
    jassert (jresponse.startsWith("y") || jresponse.startsWith("n"));
    return jresponse.startsWith("y");
}

inline File getOutputFile(const ArgumentList &args, const File &directoryWithin, const bool shouldCreateParentDirectories=true) {
    const auto outputFileEntry = args.getValueForOption("--output|-o");
    if (outputFileEntry.isEmpty()) {
        std::cout << "please specify an output file via --output|-o <output_filename>" << std::endl;
    }
    const auto outputFile = asAbsPathOrWithinDirectory(outputFileEntry, directoryWithin);

    if (const bool forceOverwrite = args.containsOption("--force|-f");
        outputFile.existsAsFile() && !forceOverwrite)
    {
        std::cout << "Warning: Output file '" << outputFile.getFullPathName()
                  << "' already exists.\n";
        std::cout << "Overwrite? (y/N): ";

        if (!checkForYesNoResponse()) // if 'no'
        {
            std::cout << "Operation cancelled.\n";
            return {};
        }
    }

    if (shouldCreateParentDirectories) {
        createParentDirectories(outputFile);
    }
    return outputFile;
}