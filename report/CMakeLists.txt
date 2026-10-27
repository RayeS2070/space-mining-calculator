# Human-readable text of core values and errors, shared by the command line
# tool and the UI.
add_library(smc_report)
target_sources(smc_report
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES
        include/report/report.hpp
        PRIVATE
        src/report.cpp
)
target_compile_features(smc_report PUBLIC cxx_std_26)
target_link_libraries(smc_report
        PUBLIC
        smc_core
        PRIVATE
        smc_warnings
)
