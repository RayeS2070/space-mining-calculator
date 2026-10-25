#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <spdlog/spdlog.h>

#include <expected>
#include <format>
#include <memory>
#include <string>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

import reader;

namespace {

// Desktop asks for OpenGL 3.0 core; the browser gets WebGL 2 (OpenGL ES 3.0).
#ifdef __EMSCRIPTEN__
constexpr auto kGlslVersion = "#version 300 es";
constexpr int kGlProfile = SDL_GL_CONTEXT_PROFILE_ES;
#else
constexpr auto kGlslVersion = "#version 130";
constexpr int kGlProfile = SDL_GL_CONTEXT_PROFILE_CORE;
#endif

// Window, GL context and ImGui of the application. SDL and ImGui are C-style
// APIs with paired init/shutdown calls; the destructor makes them in reverse.
class App {
 public:
  App() = default;
  App(const App&) = delete;
  auto operator=(const App&) -> App& = delete;
  ~App();

  auto init() -> std::expected<void, std::string>;
  auto frame() -> void;
  auto done() const -> bool { return done_; }

 private:
  struct WindowDeleter {
    auto operator()(SDL_Window* w) const -> void { SDL_DestroyWindow(w); }
  };
  struct GlContextDeleter {
    auto operator()(SDL_GLContext c) const -> void { SDL_GL_DestroyContext(c); }
  };

  auto drawUi() -> void;

  bool sdl_ = false;  // SDL_Init succeeded
  std::unique_ptr<SDL_Window, WindowDeleter> window_;
  std::unique_ptr<SDL_GLContextState, GlContextDeleter> gl_;
  bool imgui_ = false;  // ImGui context and both backends are up
  bool done_ = false;
  bool showDemo_ = false;
  core::Catalog catalog_;
};

App::~App() {
  if (imgui_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
  // Explicitly, since SDL_Quit() below must come after them.
  gl_.reset();
  window_.reset();
  if (sdl_) SDL_Quit();
}

auto App::init() -> std::expected<void, std::string> {
  auto catalog = reader::embeddedCatalog();
  if (!catalog) return std::unexpected(std::move(catalog.error()));
  catalog_ = std::move(*catalog);

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    return std::unexpected(std::format("SDL_Init: {}", SDL_GetError()));
  }
  sdl_ = true;
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, kGlProfile);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  window_.reset(SDL_CreateWindow("Space mining calculator",
                                 static_cast<int>(1280 * scale),
                                 static_cast<int>(800 * scale),
                                 SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                     SDL_WINDOW_HIGH_PIXEL_DENSITY));
  if (!window_) {
    return std::unexpected(std::format("SDL_CreateWindow: {}", SDL_GetError()));
  }
  gl_.reset(SDL_GL_CreateContext(window_.get()));
  if (!gl_) {
    return std::unexpected(
        std::format("SDL_GL_CreateContext: {}", SDL_GetError()));
  }
  SDL_GL_MakeCurrent(window_.get(), gl_.get());
  SDL_GL_SetSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef __EMSCRIPTEN__
  // No imgui.ini in the browser: there is no file system to keep it.
  io.IniFilename = nullptr;
#endif
  ImGui::StyleColorsDark();
  ImGui::GetStyle().ScaleAllSizes(scale);
  ImGui::GetStyle().FontScaleDpi = scale;

  ImGui_ImplSDL3_InitForOpenGL(window_.get(), gl_.get());
  ImGui_ImplOpenGL3_Init(kGlslVersion);
  imgui_ = true;
  return {};
}

auto App::drawUi() -> void {
  ImGui::Begin("Space mining");
  ImGui::TextUnformatted(core::kName.data(),
                         core::kName.data() + core::kName.size());
  ImGui::TextUnformatted(std::format("{} asteroids (GT5U {})",
                                     catalog_.asteroids.size(),
                                     catalog_.gt5uVersion)
                             .c_str());
  ImGui::Checkbox("ImGui demo", &showDemo_);
  ImGui::End();
  if (showDemo_) ImGui::ShowDemoWindow(&showDemo_);
}

auto App::frame() -> void {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT) done_ = true;
  }

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  drawUi();
  ImGui::Render();

  const ImGuiIO& io = ImGui::GetIO();
  glViewport(0, 0, static_cast<int>(io.DisplaySize.x),
             static_cast<int>(io.DisplaySize.y));
  glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  SDL_GL_SwapWindow(window_.get());
}

}  // namespace

auto main() -> int {
#ifdef __EMSCRIPTEN__
  // The browser keeps calling frame() after main() returns, and static
  // destructors never run there: the app lives for the whole page.
  static App app;
#else
  App app;
#endif
  if (auto ok = app.init(); !ok) {
    spdlog::error("{}", ok.error());
    return 1;
  }
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(
      [](void* arg) { static_cast<App*>(arg)->frame(); }, &app, 0, true);
#else
  while (!app.done()) app.frame();
#endif
  return 0;
}
