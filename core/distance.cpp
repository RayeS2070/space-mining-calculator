module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>

module core;

namespace core {

namespace {

// Σ w·t of the asteroids reachable at the distance. Exact in integers, so
// equal distances compare equal and plateaus come out whole.
auto weightedTime(std::span<const Asteroid> asteroids, int distance)
    -> std::int64_t {
  std::int64_t sum = 0;
  for (const Asteroid& a : asteroids) {
    if (isReachable(a, distance)) {
      sum += static_cast<std::int64_t>(a.weight) * a.duration;
    }
  }
  return sum;
}

}  // namespace

auto isReachable(const Asteroid& asteroid, int distance) -> bool {
  return asteroid.minDistance <= distance && distance <= asteroid.maxDistance;
}

auto commonDistanceRange(std::span<const Asteroid> asteroids,
                         std::span<const std::size_t> targets)
    -> std::expected<DistanceRange, DistanceError> {
  if (targets.empty()) return std::unexpected(DistanceError::noTargets);

  DistanceRange range{.min = std::numeric_limits<int>::min(),
                      .max = std::numeric_limits<int>::max()};
  for (const std::size_t i : targets) {
    const Asteroid& a = asteroids[i];
    range.min = std::max(range.min, a.minDistance);
    range.max = std::min(range.max, a.maxDistance);
  }
  if (range.min > range.max) {
    return std::unexpected(DistanceError::noCommonRange);
  }
  return range;
}

auto bestDistances(std::span<const Asteroid> asteroids,
                   std::span<const std::size_t> targets)
    -> std::expected<std::vector<DistanceRange>, DistanceError> {
  const auto common = commonDistanceRange(asteroids, targets);
  if (!common) return std::unexpected(common.error());

  std::vector<DistanceRange> best;
  std::int64_t bestTime = std::numeric_limits<std::int64_t>::max();
  for (int distance = common->min; distance <= common->max; ++distance) {
    const std::int64_t time = weightedTime(asteroids, distance);
    if (time < bestTime) {
      bestTime = time;
      best.assign({{.min = distance, .max = distance}});
    } else if (time == bestTime) {
      if (best.back().max == distance - 1) {
        best.back().max = distance;
      } else {
        best.push_back({.min = distance, .max = distance});
      }
    }
  }
  return best;
}

}  // namespace core
