#ifndef SMC_UI_PLATFORM_HPP
#define SMC_UI_PLATFORM_HPP

#include <SDL3/SDL.h>

#include <expected>
#include <memory>
#include <string>

// The window, its GL context and ImGui. SDL and ImGui are C-style APIs with
// paired init/shutdown calls; the destructor makes them in reverse.
class Platform {
 public:
  Platform() = default;
  Platform(const Platform&) = delete;
  auto operator=(const Platform&) -> Platform& = delete;
  ~Platform();

  auto init() -> std::expected<void, std::string>;

  // Handles the pending events and starts an ImGui frame.
  auto beginFrame() -> void;
  // Renders the ImGui frame and shows it.
  auto endFrame() -> void;

  // The user closed the window.
  auto done() const -> bool { return done_; }

 private:
  struct WindowDeleter {
    auto operator()(SDL_Window* w) const -> void { SDL_DestroyWindow(w); }
  };
  struct GlContextDeleter {
    auto operator()(SDL_GLContext c) const -> void { SDL_GL_DestroyContext(c); }
  };

  bool sdl_ = false;  // SDL_Init succeeded
  std::unique_ptr<SDL_Window, WindowDeleter> window_;
  std::unique_ptr<SDL_GLContextState, GlContextDeleter> gl_;
  bool imgui_ = false;  // ImGui context and both backends are up
  bool done_ = false;
};

#endif  // SMC_UI_PLATFORM_HPP
