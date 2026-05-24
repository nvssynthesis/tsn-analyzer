include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(tsn-tests
        ModernSettingsTest.cpp
)
add_executable(read-write-settings
        ReadWriteSettings.cpp
)

message(STATUS "Source lib directory: ${CMAKE_CURRENT_SOURCE_DIR}/../Source/lib")
message(STATUS "juce_utils directory: ${CMAKE_CURRENT_SOURCE_DIR}/../juce_utils")


set(COMMON_INCLUDES
        ${CMAKE_CURRENT_SOURCE_DIR}/../Source/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../juce_utils
)
target_include_directories(tsn-tests            PRIVATE ${COMMON_INCLUDES})
target_include_directories(read-write-settings  PRIVATE ${COMMON_INCLUDES})

set(COMMON_LIBS_PUBLIC
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_formats
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce_utils
)

target_link_libraries(tsn-tests
        PUBLIC ${COMMON_LIBS_PUBLIC}
        PRIVATE
        Catch2::Catch2WithMain
        tsn::analyzer
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

target_link_libraries(read-write-settings
        PUBLIC ${COMMON_LIBS_PUBLIC}
        PRIVATE
        tsn::analyzer
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# let CLion / CTest discover tests
include(CTest)
include(Catch)
catch_discover_tests(tsn-tests)