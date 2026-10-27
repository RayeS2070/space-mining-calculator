#include <spdlog/spdlog.h>

#include <expected>
#include <string>
#include <utility>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "calculator.hpp"
#include "core/model.hpp"
#include "platform.hpp"
#include "reader/reader.hpp"

namespace {

// The platform and what it shows; one object, so the browser's frame
// callback gets everything through one pointer.
class Application {
 public:
  explicit Application(core::Catalog catalog)
      : calculator_(std::move(catalog)) {}

  auto init() -> std::expected<void, std::string> { return platform_.init(); }

  auto frame() -> void {
    platform_.beginFrame();
    calculator_.draw();
    platform_.endFrame();
  }

  auto done() const -> bool { return platform_.done(); }

 private:
  Platform platform_;
  Calculator calculator_;
};

}  // namespace

auto main() -> int {
  auto catalog = reader::embeddedCatalog();
  if (!catalog) {
    spdlog::error("{}", catalog.error());
    return 1;
  }
#ifdef __EMSCRIPTEN__
  // The browser keeps calling frame() after main() returns, and static
  // destructors never run there: the application lives for the whole page.
  static Application app(std::move(*catalog));
#else
  Application app(std::move(*catalog));
#endif
  if (auto ok = app.init(); !ok) {
    spdlog::error("{}", ok.error());
    return 1;
  }
#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(
      [](void* arg) { static_cast<Application*>(arg)->frame(); }, &app, 0,
      true);
#else
  while (!app.done()) app.frame();
#endif
  return 0;
}
