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
// With the drones fixed and every target reachable, the target recipes, and
// so the numerator, do not depend on the distance: the best distances are
// those with the least Σ w·t over the candidates there.
//
// Without the drones, a rougher estimate: each asteroid the module tier
// allows counts once with its base duration, as if one drone tier reached it;
// the voltage, which depends on the drone tier, does not filter.

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
  notMinable,     // the module may pick no recipe of a target: see canMine
};

// Why the module may pick no recipe of an asteroid.
enum class Unminable {
  moduleTier,  // the module is below the asteroid's minModuleTier
  noDrone,     // no given drone tier reaches the asteroid
  voltage,     // with every given drone the recipe needs more EU/t than the
               // module has
};

// Whether the asteroid can be picked at the distance.
auto isReachable(const Asteroid& asteroid, int distance) -> bool;

// The distances where every target can be picked. targets index asteroids.
auto commonDistanceRange(std::span<const Asteroid> asteroids,
                         std::span<const std::size_t> targets)
    -> std::expected<DistanceRange, DistanceError>;

// The drone tiers that reach every target; all tiers for no targets.
auto commonDroneTiers(std::span<const Asteroid> asteroids,
                      std::span<const std::size_t> targets) -> DroneSet;

// Whether the module tier allows the asteroid with any drone.
auto fitsModule(const Asteroid& asteroid, const MinerModule& miner) -> bool;

// Whether the module may pick some recipe of the asteroid with the drones,
// at the asteroid's own distances; if not, the first reason of moduleTier,
// noDrone, voltage that holds.
auto canMine(const Asteroid& asteroid, const MinerModule& miner,
             const DroneSet& drones) -> std::expected<void, Unminable>;

// Every distance in the common range of the targets with the least Σ w·t,
// as ascending disjoint ranges: the best is often a plateau, sometimes
// several. targets index asteroids; a repeated one changes nothing.
// The rough estimate: every asteroid the module allows once, base durations.
auto bestDistances(std::span<const Asteroid> asteroids,
                   std::span<const std::size_t> targets,
                   const MinerModule& miner)
    -> std::expected<std::vector<DistanceRange>, DistanceError>;

// The same over the recipes the module may pick with settings.drones, an
// asteroid once per drone tier that reaches it; times with the plasma and
// overdrive of the settings. settings.distance is ignored: it is searched.
auto bestDistances(std::span<const Asteroid> asteroids,
                   std::span<const std::size_t> targets,
                   const MinerModule& miner, const Settings& settings)
    -> std::expected<std::vector<DistanceRange>, DistanceError>;

}  // namespace core

#endif  // SMC_CORE_DISTANCE_HPP
