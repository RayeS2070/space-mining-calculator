# Reads data/asteroids.json into the core model. Boost.JSON stays in the
# implementation unit: importers of `reader` do not see it.
add_library(smc_reader)

# The data goes into the binary at build time, so every platform, the
# browser included, gets it the same way. Editing the JSON rebuilds only the
# generated TU.
set(SMC_ASTEROIDS_JSON ${PROJECT_SOURCE_DIR}/data/asteroids.json)
set(SMC_ASTEROIDS_CPP ${CMAKE_CURRENT_BINARY_DIR}/asteroids_json.cpp)
add_custom_command(
        OUTPUT ${SMC_ASTEROIDS_CPP}
        COMMAND ${CMAKE_COMMAND}
        -DINPUT=${SMC_ASTEROIDS_JSON}
        -DOUTPUT=${SMC_ASTEROIDS_CPP}
        -DMODULE=reader
        -DNAMESPACE=reader
        -DFUNCTION=embeddedJson
        -P ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
        DEPENDS ${SMC_ASTEROIDS_JSON} ${PROJECT_SOURCE_DIR}/cmake/embed.cmake
        COMMENT "Embedding data/asteroids.json"
        VERBATIM
)

target_sources(smc_reader
        PUBLIC
        FILE_SET CXX_MODULES FILES
        reader.cppm
        PRIVATE
        reader.cpp
        ${SMC_ASTEROIDS_CPP}
)
target_compile_features(smc_reader PUBLIC cxx_std_26)
target_link_libraries(smc_reader
        PUBLIC
        smc_core
        PRIVATE
        smc_warnings
        Boost::json
)
