cmake_minimum_required(VERSION 3.30 FATAL_ERROR)

project(space-mining-calc LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

# Checks below go before the dependencies, so they get them too.

# Bounds and precondition checks in the standard library for Debug builds.
# Both keep the ABI, unlike _GLIBCXX_DEBUG, so code built with and without
# them still links together.
add_compile_definitions(
        $<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS>
        $<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG>
)

option(SMC_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
if(SMC_SANITIZE)
        add_compile_options(
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
        )
        add_link_options(-fsanitize=address,undefined)
        # Lets ASan see accesses past size() but within
        # capacity() of std::vector.
        add_compile_definitions(_GLIBCXX_SANITIZE_VECTOR)
endif()

include(cmake/CPM.cmake)

# Only the Boost libraries we use are configured:
# the full superproject has dozens.
# json reads data/asteroids.json.
set(BOOST_INCLUDE_LIBRARIES program_options json)
# Boost.Container (under Boost.JSON) header-only: its compiled part has pool
# resources JSON does not use, and one of them does not build for the
# browser. Without threads (wasm without -pthread) Boost.Config gives no
# BOOST_HAS_PTHREADS and container/detail/thread_mutex.hpp falls to its Win32
# branch.
CPMAddPackage(URI "gh:boostorg/boost#boost-1.92.0"
        OPTIONS
        "BOOST_CONTAINER_HEADER_ONLY ON"
)
CPMAddPackage(URI "gh:gabime/spdlog#v1.17.0"
        OPTIONS
        "SPDLOG_USE_STD_FORMAT ON"
        "SPDLOG_NO_TLS ON"
)

# Warnings for our own targets only; the dependencies keep their own flags.
add_library(smc_warnings INTERFACE)
target_compile_options(smc_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -pedantic>
)

add_subdirectory(core)
add_subdirectory(reader)

# The command line tool needs a terminal: not for the browser.
if(NOT EMSCRIPTEN)
    add_executable(space_mining_calc src/main.cpp)
    target_compile_features(space_mining_calc PRIVATE cxx_std_26)
    target_link_libraries(space_mining_calc
            PRIVATE
            smc_warnings
            smc_reader
            Boost::program_options
            spdlog::spdlog
    )
endif()

add_subdirectory(ui)
