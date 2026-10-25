# The model and the optimization: no UI, no I/O. The command line tool and
# the UI are thin shells over it. Headers are included as "core/<name>.hpp".
add_library(smc_core)
target_sources(smc_core
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}
        FILES
        model.hpp
        mechanics.hpp
        distance.hpp
        PRIVATE
        model.cpp
        mechanics.cpp
        distance.cpp
)
target_compile_features(smc_core PUBLIC cxx_std_26)
target_link_libraries(smc_core PRIVATE smc_warnings)
