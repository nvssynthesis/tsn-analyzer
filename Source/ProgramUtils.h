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

inline File getOutputFile(const ArgumentList &args) {
    const auto outputFlagIter = std::ranges::find(args.arguments.begin()+1,
        args.arguments.end(),
        "-o",
        [](const auto &x) {
        return x.text;
    });
    if (outputFlagIter == args.arguments.end()) {
        std::cerr << "Could not find output file; should be preceded by '-o'" << std::endl;
        return {};
    }
    const auto outputFileArgIter = std::next(outputFlagIter);
    if (outputFileArgIter == args.arguments.end()) {
        std::cerr << "-o must be followed by an output filname" << std::endl;
        return {};
    }
    auto outputFile = File::getCurrentWorkingDirectory().getChildFile( outputFileArgIter->text );
    std::cout << outputFile.getFullPathName() << std::endl;


    if (const bool forceOverwrite = args.containsOption("--force|-f");
        outputFile.existsAsFile() && !forceOverwrite)
    {
        std::cout << "Warning: Output file '" << outputFile.getFullPathName()
                  << "' already exists.\n";
        std::cout << "Overwrite? (y/N): ";

        std::string response;
        std::getline(std::cin, response);

        if (response.empty() || (response[0] != 'y' && response[0] != 'Y'))
        {
            std::cout << "Operation cancelled.\n";
            return {};
        }
    }

    return outputFile;
}