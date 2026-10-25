#include "core/model.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace core {

auto findAsteroid(std::span<const Asteroid> asteroids, std::string_view name)
    -> std::optional<std::size_t> {
  for (std::size_t i = 0; i < asteroids.size(); ++i) {
    if (asteroids[i].name == name) return i;
  }
  return std::nullopt;
}

}  // namespace core
