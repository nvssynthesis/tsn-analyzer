cmake_minimum_required(VERSION 3.15)

project(tsn-analyzer VERSION 0.2.3)

configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/version.h"
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

add_subdirectory(lib)

# ============================================================================
# Console Application
# ============================================================================
juce_add_console_app(tsn_analyze
        PRODUCT_NAME "TSN Analyzer"
        COMPANY_NAME "CorrodeAudio"
)

juce_generate_juce_header(tsn_analyze)

target_sources(tsn_analyze
        PRIVATE
        Main.cpp
        AnalysisProgram.cpp
        SettingsPrograms.cpp
)

add_dependencies(tsn_analyze essentia_external)

target_include_directories(tsn_analyze
        PUBLIC
        "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(tsn_analyze
        PRIVATE
        tsn::analyzer   # :: syntax tells cmake this MUST be a target rather than a system or library name. good practice to use when linking to my own libs.
        juce_utils
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