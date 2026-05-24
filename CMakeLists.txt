cmake_minimum_required(VERSION 3.15)

project(tsn-analyzer-root VERSION 0.1.1)

# Order matters: juce_utils must come first to provide JUCE
add_subdirectory(juce_utils)
add_subdirectory(Source)
add_subdirectory(Tests)