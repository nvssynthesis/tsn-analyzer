cmake_minimum_required(VERSION 3.15)

project(tsn-analyzer-lib VERSION 0.1.0)

include(ExternalProject)

# Options
option(USE_SYSTEM_LIBRARIES "Use system-installed libraries instead of fetching" OFF)

# ============================================================================
# Find Eigen3
# ============================================================================
find_package(Eigen3 REQUIRED NO_MODULE)

if(TARGET Eigen3::Eigen)
    get_target_property(EIGEN_INCLUDE_DIR Eigen3::Eigen INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found Eigen3: ${EIGEN_INCLUDE_DIR}")
else()
    find_path(EIGEN_INCLUDE_DIR
            NAMES Eigen/Dense
            PATHS
            /opt/homebrew/include/eigen3
            /usr/local/include/eigen3
            /usr/include/eigen3
            NO_DEFAULT_PATH
    )
    if(EIGEN_INCLUDE_DIR)
        message(STATUS "Found Eigen3 headers: ${EIGEN_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Could not find Eigen3 headers")
    endif()
endif()

# ============================================================================
# Essentia Setup
# ============================================================================
include(FetchContent)

FetchContent_Declare(
        essentia
        GIT_REPOSITORY https://github.com/MTG/essentia.git
        GIT_SHALLOW TRUE
        GIT_SUBMODULES ""  # Disable submodules
)
FetchContent_MakeAvailable(essentia)

set(ESSENTIA_DIR "${essentia_SOURCE_DIR}")
set(ESSENTIA_INSTALL_DIR "${CMAKE_BINARY_DIR}/essentia_install")

# ============================================================================
# Build Essentia
# ============================================================================
message(STATUS "Building Essentia with waf...")

find_program(PYTHON3_EXECUTABLE
        NAMES python3.11 python3.10 python3.9 python3.8 python3
        DOC "Python 3 interpreter for building Essentia"
)

if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "Could not find Python 3 executable")
endif()

message(STATUS "Using Python: ${PYTHON3_EXECUTABLE}")

# Set up proper environment for macOS
if(APPLE)
    execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    message(STATUS "macOS SDK path: ${MACOS_SDK_PATH}")

    set(ESSENTIA_ENV
            "SDKROOT=${MACOS_SDK_PATH}"
            "CPPFLAGS=-isysroot ${MACOS_SDK_PATH} -I${EIGEN_INCLUDE_DIR}"
            "CFLAGS=-isysroot ${MACOS_SDK_PATH}"
            "CXXFLAGS=-isysroot ${MACOS_SDK_PATH} -I${EIGEN_INCLUDE_DIR}"
            "LDFLAGS=-isysroot ${MACOS_SDK_PATH}"
    )
else()
    set(ESSENTIA_ENV
            "CPPFLAGS=-I${EIGEN_INCLUDE_DIR}"
            "CXXFLAGS=-I${EIGEN_INCLUDE_DIR}"
    )
endif()

message(STATUS "Using Eigen from: ${EIGEN_INCLUDE_DIR}")

if(NOT CMAKE_BUILD_PARALLEL_LEVEL)
    set(CMAKE_BUILD_PARALLEL_LEVEL 4)
endif()

# Build Essentia using ExternalProject
ExternalProject_Add(essentia_external
        SOURCE_DIR ${ESSENTIA_DIR}
        DOWNLOAD_COMMAND ""
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${ESSENTIA_ENV}
        ${PYTHON3_EXECUTABLE} waf configure
        --build-static
        --prefix=${ESSENTIA_INSTALL_DIR}
        --lightweight=libsamplerate
        --fft=ACCELERATE
        --no-msse
        BUILD_COMMAND ${CMAKE_COMMAND} -E env ${ESSENTIA_ENV}
        ${PYTHON3_EXECUTABLE} waf -j ${CMAKE_BUILD_PARALLEL_LEVEL}
        INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${ESSENTIA_ENV}
        ${PYTHON3_EXECUTABLE} waf install
        BUILD_IN_SOURCE 1
        BUILD_ALWAYS 0
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
)

# Set up Essentia include directories
set(ESSENTIA_INCLUDE_DIRS
        "${ESSENTIA_DIR}/src"
        "${EIGEN_INCLUDE_DIR}"
)

if(EXISTS "${ESSENTIA_INSTALL_DIR}/include")
    list(APPEND ESSENTIA_INCLUDE_DIRS "${ESSENTIA_INSTALL_DIR}/include")
    if(EXISTS "${ESSENTIA_INSTALL_DIR}/include/essentia")
        list(APPEND ESSENTIA_INCLUDE_DIRS "${ESSENTIA_INSTALL_DIR}/include/essentia")
    endif()
endif()

# Platform-specific system libraries for Essentia
if(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    find_library(CORE_AUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox)

    set(ESSENTIA_SYSTEM_LIBRARIES
            ${ACCELERATE_FRAMEWORK}
            ${CORE_AUDIO_FRAMEWORK}
            ${AUDIO_TOOLBOX_FRAMEWORK}
    )

    find_library(FFTW3_LIB fftw3 PATHS /usr/local/lib /opt/local/lib /opt/homebrew/lib)
    find_library(FFTW3F_LIB fftw3f PATHS /usr/local/lib /opt/local/lib /opt/homebrew/lib)
    find_library(SAMPLERATE_LIB samplerate PATHS /usr/local/lib /opt/local/lib /opt/homebrew/lib)

    if(FFTW3_LIB AND FFTW3F_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${FFTW3_LIB} ${FFTW3F_LIB})
        message(STATUS "Found FFTW3 libraries: ${FFTW3_LIB}, ${FFTW3F_LIB}")
    else()
        message(STATUS "FFTW3 not found, using built-in FFT")
    endif()

    if(SAMPLERATE_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${SAMPLERATE_LIB})
        message(STATUS "Found libsamplerate: ${SAMPLERATE_LIB}")
    else()
        message(FATAL_ERROR "libsamplerate not found, but required for Resample algorithm")
    endif()
elseif(UNIX)
    set(ESSENTIA_SYSTEM_LIBRARIES pthread m)

    find_library(FFTW3_LIB fftw3)
    find_library(FFTW3F_LIB fftw3f)
    find_library(SAMPLERATE_LIB samplerate)

    if(FFTW3_LIB AND FFTW3F_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${FFTW3_LIB} ${FFTW3F_LIB})
    endif()

    if(SAMPLERATE_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${SAMPLERATE_LIB})
    else()
        message(FATAL_ERROR "libsamplerate not found, but required for Resample algorithm")
    endif()
endif()

set(ESSENTIA_LIBRARY "${ESSENTIA_INSTALL_DIR}/lib/libessentia.a")

# Create imported target for Essentia
add_library(essentia_built STATIC IMPORTED GLOBAL)
set_target_properties(essentia_built PROPERTIES
        IMPORTED_LOCATION "${ESSENTIA_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ESSENTIA_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${ESSENTIA_SYSTEM_LIBRARIES}"
)
add_dependencies(essentia_built essentia_external)

if(NOT EXISTS "${ESSENTIA_LIBRARY}")
    message(WARNING
            "Essentia library not found at: ${ESSENTIA_LIBRARY}\n"
            "If build fails, run: cmake --build . --target essentia_external\n"
            "Then build again."
    )
endif()

# ============================================================================
# TSN Analyzer Library
# ============================================================================

# Collect all source files
file(GLOB_RECURSE TSN_ANALYZER_SOURCES
        "./*.cpp"
        "./*.h"
)

# Create the library
add_library(tsn_analyzer STATIC ${TSN_ANALYZER_SOURCES})
add_library(tsn::analyzer ALIAS tsn_analyzer)

# Add dependency on Essentia build
add_dependencies(tsn_analyzer essentia_external)

message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}/juce_utils")

# Include directories
target_include_directories(tsn_analyzer
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        # Add the parent build directory where JuceHeader.h gets generated by the app
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/tsn_analyzer_app_artefacts/JuceLibraryCode>
        ${ESSENTIA_INCLUDE_DIRS}
        ${EIGEN_INCLUDE_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../../juce_utils
)

# Link libraries
target_link_libraries(tsn_analyzer
        PUBLIC
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce_utils
        fmt::fmt
        PRIVATE
        essentia_built
        Eigen3::Eigen
)

# Compiler features
target_compile_features(tsn_analyzer PUBLIC cxx_std_20)

# Platform-specific settings
if(APPLE)
    target_compile_options(tsn_analyzer PRIVATE
            -Wall -Wextra -Wshadow -Wuninitialized
    )
endif()

message(STATUS "TSN Analyzer Library Configuration:")
message(STATUS "  Essentia directory: ${ESSENTIA_DIR}")
message(STATUS "  Essentia library: ${ESSENTIA_LIBRARY}")
message(STATUS "  Eigen include: ${EIGEN_INCLUDE_DIR}")