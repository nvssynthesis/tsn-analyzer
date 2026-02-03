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

inline File getOutputFile(const ArgumentList &args)
{
    if (args.arguments.size() < 3) {
        return {};
    }
    auto outputFile = File::getCurrentWorkingDirectory()
                          .getChildFile(args.arguments[2].text);

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