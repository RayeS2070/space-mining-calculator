# The calculator's UI: Dear ImGui over SDL3, OpenGL 3 on the desktop and
# WebGL 2 in the browser.

# SDL3 is built from source on every platform: a regular target, so the
# build graph knows its headers before anything is compiled.
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

# ImGui has no CMake build of its own: the core and the two backends.
# imgui_demo.cpp stays in for the empty ShowDemoWindow() and friends that
# IMGUI_DISABLE_DEMO_WINDOWS leaves there; imgui.cpp still refers to them.
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
target_compile_definitions(imgui
        PUBLIC
        IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        IMGUI_DISABLE_DEMO_WINDOWS
)
# Without it CMake passes the compiler's default standard, C++98 for em++.
target_compile_features(imgui PUBLIC cxx_std_17)

target_link_libraries(imgui PUBLIC SDL3::SDL3)
if(NOT EMSCRIPTEN)
    find_package(OpenGL REQUIRED)
    target_link_libraries(imgui PUBLIC OpenGL::GL)
endif()

add_executable(smc_ui
        src/main.cpp
        src/platform.hpp
        src/platform.cpp
        src/calculator.hpp
        src/calculator.cpp
)
target_compile_features(smc_ui PRIVATE cxx_std_26)
target_link_libraries(smc_ui
        PRIVATE
        smc_warnings
        smc_reader
        smc_report
        imgui
        spdlog::spdlog
)

if(EMSCRIPTEN)
    # smc_ui.html with smc_ui.js and smc_ui.wasm next to it.
    set_target_properties(smc_ui PROPERTIES SUFFIX ".html")
    target_link_options(smc_ui
            PRIVATE
            -sALLOW_MEMORY_GROWTH=1
            -sMIN_WEBGL_VERSION=2
            -sMAX_WEBGL_VERSION=2
            --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html
    )
    set_property(TARGET smc_ui APPEND PROPERTY LINK_DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/shell.html)

    # The browser build is a site: at the top of the prefix, the page as
    # index.html, so the directory can be served as it is (GitHub Pages too).
    # The page loads smc_ui.js by name, which keeps it.
    install(FILES $<TARGET_FILE:smc_ui> DESTINATION . RENAME index.html)
    install(FILES
            $<TARGET_FILE_DIR:smc_ui>/smc_ui.js
            $<TARGET_FILE_DIR:smc_ui>/smc_ui.wasm
            DESTINATION .
    )
else()
    install(TARGETS smc_ui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
