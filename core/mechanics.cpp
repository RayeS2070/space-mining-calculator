#include "core/mechanics.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace core {

auto getEnergyConsumption(const Asteroid& asteroid, const double speedup)
    -> std::int64_t {
  return asteroid.scaleEUt ? static_cast<std::int64_t>(std::ceil(
                                 static_cast<double>(asteroid.eut) * speedup))
                           : asteroid.eut;
}

auto expand(const Asteroid& asteroid, std::size_t index)
    -> std::vector<Recipe> {
  std::vector<Recipe> recipes;
  for (int tier = asteroid.firstDroneTier; tier <= asteroid.lastDroneTier;
       ++tier) {
    const int k = tier - asteroid.firstDroneTier;
    const int extraSize = (1 << k) - 1;
    const double speedup = std::sqrt(static_cast<double>(k + 1));
    recipes.push_back({
        .asteroid = index,
        .droneTier = tier,
        .minSize = asteroid.minSize + extraSize,
        .maxSize = asteroid.maxSize + extraSize,
        .duration = static_cast<int>(
            std::ceil(static_cast<double>(asteroid.duration) / speedup)),
        .eut = getEnergyConsumption(asteroid, speedup),
        .minDistance = asteroid.minDistance,
        .maxDistance = asteroid.maxDistance,
        .computation = asteroid.computation,
        .minModuleTier = asteroid.minModuleTier,
        .weight = asteroid.weight,
    });
  }
  return recipes;
}

auto expand(std::span<const Asteroid> asteroids) -> std::vector<Recipe> {
  std::vector<Recipe> recipes;
  for (std::size_t i = 0; i < asteroids.size(); ++i) {
    recipes.append_range(expand(asteroids[i], i));
  }
  return recipes;
}

auto moduleVoltage(const MinerModule& miner) -> std::int64_t {
  return kModuleVoltage[static_cast<std::size_t>(miner.tier - 1)];
}

auto isCandidate(const Recipe& recipe, const MinerModule& miner,
                 const Settings& settings) -> bool {
  return settings.drones.test(static_cast<std::size_t>(recipe.droneTier)) &&
         recipe.minModuleTier <= miner.tier &&
         recipe.minDistance <= settings.distance &&
         settings.distance <= recipe.maxDistance &&
         recipe.eut <= moduleVoltage(miner);
}

auto bonusChance(int plasmaTier, double overdrive) -> int {
  if (plasmaTier <= 0 || plasmaTier > kPlasmaTierCount) return 0;
  // GTUtility.powInt(x, 3) multiplies as x * (x * x): same rounding here.
  const double x = static_cast<double>(plasmaTier) / 6;
  const double cube = x * (x * x);
  return std::min(static_cast<int>(cube * 10000 * (2.0 - overdrive)),
                  kMaxBonusChance);
}

auto recipeTime(int duration, int plasmaTier, double overdrive) -> int {
  if (plasmaTier <= 0) return duration;
  const double multiplier =
      std::max((1.0 - 0.1 * (plasmaTier - 1)) / overdrive, kMinTimeMultiplier);
  return static_cast<int>(static_cast<double>(duration) * multiplier);
}

auto outputHits(std::span<const Output> outputs) -> std::vector<int> {
  int total = 0;
  for (const Output& o : outputs) total += o.chance;
  std::vector<int> hits;
  hits.reserve(outputs.size());
  int cumulative = 0;
  int previous = -1;  // the last random value already taken
  for (const Output& o : outputs) {
    cumulative += o.chance;
    const int last = std::min(cumulative, total - 1);
    hits.push_back(std::max(last - previous, 0));
    previous = std::max(previous, last);
  }
  return hits;
}

auto expectedDraws(const Recipe& recipe, int bonusChance) -> double {
  return recipe.minSize +
         (recipe.maxSize - recipe.minSize) * (bonusChance / 10000.0);
}

}  // namespace core
