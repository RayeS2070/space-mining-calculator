#ifndef SMC_CORE_DISTANCE_HPP
#define SMC_CORE_DISTANCE_HPP

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

#include "core/model.hpp"

// Choice of the mining distance for chosen target asteroids.
//
// The yield per tick is Σ w·E / Σ w·t over the candidate recipes (README).
// While every target is reachable, the numerator does not depend on the
// distance, so the best distances are those with the least Σ w·t: the
// summed weight·time of everything reachable there.
//
// Simplified so far: each asteroid counts once with its base duration, as if
// one drone tier reached it; module tier, voltage and mods do not filter.

namespace core {

// Distances from min to max, both included.
struct DistanceRange {
  int min = 0;
  int max = 0;

  friend auto operator==(const DistanceRange&, const DistanceRange&)
      -> bool = default;
};

enum class DistanceError {
  noTargets,      // no target asteroid given
  noCommonRange,  // the targets share no distance
};

// Whether the asteroid can be picked at the distance.
auto isReachable(const Asteroid& asteroid, int distance) -> bool;

// The distances where every target can be picked. targets index asteroids.
auto commonDistanceRange(std::span<const Asteroid> asteroids,
                         std::span<const std::size_t> targets)
    -> std::expected<DistanceRange, DistanceError>;

// Every distance in the common range of the targets with the least Σ w·t,
// as ascending disjoint ranges: the best is often a plateau, sometimes
// several. targets index asteroids; a repeated one changes nothing.
auto bestDistances(std::span<const Asteroid> asteroids,
                   std::span<const std::size_t> targets)
    -> std::expected<std::vector<DistanceRange>, DistanceError>;

}  // namespace core

#endif  // SMC_CORE_DISTANCE_HPP
