# The model and the optimization: no UI, no I/O. The command line tool and
# the UI are thin shells over it.
add_library(smc_core)
target_sources(smc_core
        PUBLIC
        FILE_SET CXX_MODULES FILES
        core.cppm
        model.cppm
        mechanics.cppm
        PRIVATE
        mechanics.cpp
)
target_compile_features(smc_core PUBLIC cxx_std_26)
target_link_libraries(smc_core PRIVATE smc_warnings)
