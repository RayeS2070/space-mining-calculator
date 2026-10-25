#ifndef SMC_CORE_MECHANICS_HPP
#define SMC_CORE_MECHANICS_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/model.hpp"

// The mechanics of the mining module, as read from GT5U:
// TileEntityModuleMiner.java and SpaceMiningRecipes.java. The functions
// repeat the Java arithmetic step by step, roundings included, so the model
// matches the game to the last tick.

namespace core {

// Bonus draw chance cap, out of 10000.
inline constexpr int kMaxBonusChance = 7500;
// Lower bound of the recipe time multiplier.
inline constexpr double kMinTimeMultiplier = 0.5;

// The recipes of one asteroid, one per drone tier, as addRecipesToDrones
// adds them. index is the asteroid's place in Catalog::asteroids.
auto expand(const Asteroid& asteroid, std::size_t index) -> std::vector<Recipe>;
auto expand(std::span<const Asteroid> asteroids) -> std::vector<Recipe>;

// Max input voltage of the module, EU/t: set by its tier, 1..kModuleTierCount.
auto moduleVoltage(const MinerModule& miner) -> std::int64_t;

// Whether the module may pick the recipe. Inputs other than the drone (drill
// tips, rods, plasma) are assumed present.
auto isCandidate(const Recipe& recipe, const MinerModule& miner,
                 const Settings& settings) -> bool;

// Chance of each draw past minSize, out of 10000: getBonusStackChance.
auto bonusChance(int plasmaTier, double overdrive) -> int;

// Recipe time in ticks: getRecipeTime. Rounds down, so it is a step function
// of the overdrive. Needs overdrive > 0: the game takes 0 as valid and gets an
// infinite time.
auto recipeTime(int duration, int plasmaTier, double overdrive) -> int;

// How many of the values of nextInt(Σ chance) select each output. The game
// picks the first j with random <= Σ_{i<=j} chance_i, so the first output gets
// one value more than its chance and the last one value less.
auto outputHits(std::span<const Output> outputs) -> std::vector<int>;

// Expected draws per parallel: minSize always, the rest with the bonus chance.
auto expectedDraws(const Recipe& recipe, int bonusChance) -> double;

}  // namespace core

#endif  // SMC_CORE_MECHANICS_HPP
