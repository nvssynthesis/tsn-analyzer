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

target_link_libraries(tsn-tests PRIVATE
        Catch2::Catch2WithMain   # provides main()
        tsn::analyzer
)

# let CLion / CTest discover tests
include(CTest)
include(Catch)
catch_discover_tests(tsn-tests)