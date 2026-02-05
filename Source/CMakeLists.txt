cmake_minimum_required(VERSION 3.15)

project(tsn-analyzer VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# JUCE setup options
set(JUCE_DIR "" CACHE PATH "Path to JUCE framework (leave empty to fetch)")
option(JUCE_FETCH_IF_MISSING "Automatically fetch JUCE if JUCE_DIR is not set" ON)
set(JUCE_VERSION "8.0.10" CACHE STRING "JUCE version to fetch if JUCE_FETCH_IF_MISSING is ON")

# ============================================================================
# JUCE Setup
# ============================================================================
if(JUCE_DIR)
    message(STATUS "Using user-specified JUCE from: ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} juce_build)
elseif(JUCE_FETCH_IF_MISSING)
    message(STATUS "Fetching JUCE ${JUCE_VERSION}...")
    FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG ${JUCE_VERSION}
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(JUCE)
else()
    message(FATAL_ERROR
            "JUCE not found!\n"
            "Please either:\n"
            "  1. Set JUCE_DIR to your JUCE installation path, or\n"
            "  2. Enable JUCE_FETCH_IF_MISSING to auto-download JUCE"
    )
endif()

# ============================================================================
# Add the analysis library subdirectory
# ============================================================================
add_subdirectory(lib)

# ============================================================================
# Console Application
# ============================================================================
juce_add_console_app(tsn_analyze
        PRODUCT_NAME "TSN Analyzer"
        COMPANY_NAME "CorrodeAudio"
)

juce_generate_juce_header(tsn_analyze)

#target_sources(AnalysisProgram

target_sources(tsn_analyze
        PRIVATE
        Main.cpp
        AnalysisProgram.cpp
)

add_dependencies(tsn_analyze essentia_external)

target_include_directories(tsn_analyze
        PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}/lib"
)

target_link_libraries(tsn_analyze
        PRIVATE
        tsn::analyzer   # :: syntax tells cmake this MUST be a target rather than a system or library name. good practice to use when linking to my own libs.
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# Platform-specific settings
if(APPLE)
    target_compile_options(tsn_analyze PRIVATE
            -Wall -Wextra -Wshadow -Wuninitialized
    )
endif()

message(STATUS "TSN Analyzer Console App Configuration Complete")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET tsn_analyze POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink
            $<TARGET_FILE:tsn_analyze>
            $ENV{HOME}/bin/tsn_analyze
            COMMENT "Symlinking to ~/bin"
    )
endif()