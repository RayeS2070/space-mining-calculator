# Unit tests of core and report, on synthetic asteroids and on the embedded
# data.
CPMAddPackage(URI "gh:google/googletest#v1.18.0"
        OPTIONS
        "INSTALL_GTEST OFF"
        "BUILD_GMOCK OFF"
)
include(GoogleTest)

add_executable(smc_tests distance_test.cpp report_test.cpp)
target_link_libraries(smc_tests
        PRIVATE
        smc_warnings
        smc_reader
        smc_report
        GTest::gtest_main
)
gtest_discover_tests(smc_tests)
