# SDL3 is built from source on every platform: a regular target, so the
# build graph knows its headers before anything is compiled.
# Desktop renders with OpenGL 3, the browser with WebGL 2.

CPMAddPackage(URI "gh:libsdl-org/SDL#release-3.4.18"
        OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TEST_LIBRARY OFF"
        "SDL_TESTS OFF"
        "SDL_EXAMPLES OFF"
        "SDL_INSTALL OFF"
)
CPMAddPackage("gh:ocornut/imgui#v1.93.0")

add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui
        PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)
target_compile_definitions(imgui PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)
# Without it CMake passes the compiler's default standard, C++98 for em++.
target_compile_features(imgui PUBLIC cxx_std_17)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
if(NOT EMSCRIPTEN)
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui PUBLIC OpenGL::GL)
endif()

add_executable(space_mining_calc_ui main.cpp)
target_compile_features(space_mining_calc_ui PRIVATE cxx_std_26)
target_link_libraries(space_mining_calc_ui
        PRIVATE
        smc_warnings
        smc_reader
        imgui
        spdlog::spdlog
)

if(EMSCRIPTEN)
    set_target_properties(space_mining_calc_ui PROPERTIES SUFFIX ".html")
    target_link_options(space_mining_calc_ui
            PRIVATE
            -sALLOW_MEMORY_GROWTH=1
            -sMIN_WEBGL_VERSION=2
            -sMAX_WEBGL_VERSION=2
            --shell-file
            ${imgui_SOURCE_DIR}/examples/libs/emscripten/shell_minimal.html
    )
endif()
