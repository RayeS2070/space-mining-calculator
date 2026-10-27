#ifndef SMC_REPORT_REPORT_HPP
#define SMC_REPORT_REPORT_HPP

#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/distance.hpp"
#include "core/model.hpp"

// Human-readable text of core values and errors, shared by the command line
// tool and the UI so both say the same thing.

namespace report {

// "a, b, c".
auto join(std::ranges::input_range auto&& parts) -> std::string {
  return parts | std::views::join_with(std::string_view(", ")) |
         std::ranges::to<std::string>();
}

// "5" or "1-40".
auto formatRange(const core::DistanceRange& range) -> std::string;

// The asteroid's distances: "1-40".
auto formatDistances(const core::Asteroid& asteroid) -> std::string;

// The drone tiers that reach the asteroid: "LV-ZPM".
auto formatDroneRange(const core::Asteroid& asteroid) -> std::string;

// Runs of adjacent tiers: "LV-IV, UV"; "none" for an empty set.
auto formatDrones(const core::DroneSet& drones) -> std::string;

// The tier names in the set, in tier order.
auto droneNames(const core::DroneSet& drones) -> std::vector<std::string_view>;

// "Coal ore x64 70.0%", one per output; the share is the game's exact one,
// see core::outputShares.
auto formatOutputs(const core::Asteroid& asteroid) -> std::vector<std::string>;

// Why the module cannot mine the asteroid ("needs a T3 module"); nothing
// when it can. Without drones only the module tier is checked, as in the
// rough estimate of core::bestDistances.
auto whyUnminable(const core::Asteroid& asteroid,
                  const core::MinerModule& miner,
                  const std::optional<core::DroneSet>& drones)
    -> std::optional<std::string>;

// The error of core::bestDistances for these targets, as one sentence.
auto describe(core::DistanceError error, const core::Catalog& catalog,
              std::span<const std::size_t> targets,
              const core::MinerModule& miner,
              const std::optional<core::DroneSet>& drones) -> std::string;

}  // namespace report

#endif  // SMC_REPORT_REPORT_HPP
