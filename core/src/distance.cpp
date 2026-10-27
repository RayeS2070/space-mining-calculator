#include "core/distance.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>

#include "core/mechanics.hpp"

namespace core {

namespace {

// Σ w·t of the asteroids the module allows, reachable at the distance.
// Exact in integers, so equal distances compare equal and plateaus come out
// whole.
auto weightedTime(std::span<const Asteroid> asteroids, const MinerModule& miner,
                  int distance) -> std::int64_t {
  std::int64_t sum = 0;
  for (const Asteroid& a : asteroids) {
    if (fitsModule(a, miner) && isReachable(a, distance)) {
      sum += static_cast<std::int64_t>(a.weight) * a.duration;
    }
  }
  return sum;
}

// Σ w·t of the recipes the module may pick with the settings.
auto weightedTime(std::span<const Recipe> recipes, const MinerModule& miner,
                  const Settings& settings) -> std::int64_t {
  std::int64_t sum = 0;
  for (const Recipe& r : recipes) {
    if (isCandidate(r, miner, settings)) {
      sum += static_cast<std::int64_t>(r.weight) *
             recipeTime(r.duration, settings.plasmaTier, settings.overdrive);
    }
  }
  return sum;
}

// The distances of the range with the least time(distance), as ascending
// disjoint ranges.
auto leastTime(const DistanceRange& range, auto time)
    -> std::vector<DistanceRange> {
  std::vector<DistanceRange> best;
  std::int64_t bestTime = std::numeric_limits<std::int64_t>::max();
  for (int distance = range.min; distance <= range.max; ++distance) {
    const std::int64_t t = time(distance);
    if (t < bestTime) {
      bestTime = t;
      best.assign({{.min = distance, .max = distance}});
    } else if (t == bestTime) {
      if (best.back().max == distance - 1) {
        best.back().max = distance;
      } else {
        best.push_back({.min = distance, .max = distance});
      }
    }
  }
  return best;
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

auto commonDroneTiers(std::span<const Asteroid> asteroids,
                      std::span<const std::size_t> targets) -> DroneSet {
  DroneSet common;
  common.set();
  for (const std::size_t i : targets) {
    const Asteroid& a = asteroids[i];
    DroneSet reach;
    for (int tier = a.firstDroneTier; tier <= a.lastDroneTier; ++tier) {
      reach.set(static_cast<std::size_t>(tier));
    }
    common &= reach;
  }
  return common;
}

auto fitsModule(const Asteroid& asteroid, const MinerModule& miner) -> bool {
  return asteroid.minModuleTier <= miner.tier;
}

auto canMine(const Asteroid& asteroid, const MinerModule& miner,
             const DroneSet& drones) -> std::expected<void, Unminable> {
  if (!fitsModule(asteroid, miner)) {
    return std::unexpected(Unminable::moduleTier);
  }
  // isCandidate decides, so the answer cannot drift from it; with the module
  // tier passed and a drone given, only the voltage is left to fail.
  const Settings settings{.distance = asteroid.minDistance, .drones = drones};
  bool anyDrone = false;
  for (const Recipe& r : expand(asteroid, 0)) {
    if (!drones.test(static_cast<std::size_t>(r.droneTier))) continue;
    anyDrone = true;
    if (isCandidate(r, miner, settings)) return {};
  }
  return std::unexpected(anyDrone ? Unminable::voltage : Unminable::noDrone);
}

auto bestDistances(std::span<const Asteroid> asteroids,
                   std::span<const std::size_t> targets,
                   const MinerModule& miner)
    -> std::expected<std::vector<DistanceRange>, DistanceError> {
  const auto common = commonDistanceRange(asteroids, targets);
  if (!common) return std::unexpected(common.error());
  for (const std::size_t i : targets) {
    if (!fitsModule(asteroids[i], miner)) {
      return std::unexpected(DistanceError::notMinable);
    }
  }

  return leastTime(*common, [&](int distance) {
    return weightedTime(asteroids, miner, distance);
  });
}

auto bestDistances(std::span<const Asteroid> asteroids,
                   std::span<const std::size_t> targets,
                   const MinerModule& miner, const Settings& settings)
    -> std::expected<std::vector<DistanceRange>, DistanceError> {
  const auto common = commonDistanceRange(asteroids, targets);
  if (!common) return std::unexpected(common.error());
  for (const std::size_t i : targets) {
    if (!canMine(asteroids[i], miner, settings.drones)) {
      return std::unexpected(DistanceError::notMinable);
    }
  }

  const std::vector<Recipe> recipes = expand(asteroids);
  Settings at = settings;
  return leastTime(*common, [&](int distance) {
    at.distance = distance;
    return weightedTime(recipes, miner, at);
  });
}

}  // namespace core
