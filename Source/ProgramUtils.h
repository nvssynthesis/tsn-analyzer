//
// Created by Nicholas Solem on 2/3/26.
//

#pragma once
#include <JuceHeader.h>

inline File getInputFile(const ArgumentList &args)
{
    if (args.arguments.size() < 2)
        return {};
    return args.arguments[1].resolveAsExistingFile();
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

inline File getOutputFile(const ArgumentList &args, const File &directoryWithin) {
    const auto outputFlagIter = std::ranges::find(std::next(args.arguments.begin()),
        args.arguments.end(),
        "-o",
        [](const auto &x) {
        return x.text;
    });
    if (outputFlagIter == args.arguments.end()) {
        std::cerr << "Could not find output filename; should be preceded by '-o'" << std::endl;
        return {};
    }
    const auto outputFileArgIter = std::next(outputFlagIter);
    if (outputFileArgIter == args.arguments.end()) {
        std::cerr << "-o must be followed by an output filename" << std::endl;
        return {};
    }
    auto outputFile = directoryWithin.getChildFile( outputFileArgIter->text );

    if (const bool forceOverwrite = args.containsOption("--force|-f");
        outputFile.existsAsFile() && !forceOverwrite)
    {
        std::cout << "Warning: Output file '" << outputFile.getFullPathName()
                  << "' already exists.\n";
        std::cout << "Overwrite? (y/N): ";

        if (checkForYesNoResponse())
        {
            std::cout << "Operation cancelled.\n";
            return {};
        }
    }

    return outputFile;
}