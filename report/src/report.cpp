#include "report/report.hpp"

#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/distance.hpp"
#include "core/mechanics.hpp"
#include "core/model.hpp"

namespace report {

auto formatRange(const core::DistanceRange& range) -> std::string {
  return range.min == range.max ? std::format("{}", range.min)
                                : std::format("{}-{}", range.min, range.max);
}

auto formatDistances(const core::Asteroid& asteroid) -> std::string {
  return formatRange(
      {.min = asteroid.minDistance, .max = asteroid.maxDistance});
}

auto formatDroneRange(const core::Asteroid& asteroid) -> std::string {
  return std::format("{}-{}", core::kDroneTierNames[asteroid.firstDroneTier],
                     core::kDroneTierNames[asteroid.lastDroneTier]);
}

auto formatDrones(const core::DroneSet& drones) -> std::string {
  std::vector<std::string> runs;
  for (int tier = 0; tier < core::kDroneTierCount; ++tier) {
    if (!drones.test(static_cast<std::size_t>(tier))) continue;
    int last = tier;
    while (last + 1 < core::kDroneTierCount &&
           drones.test(static_cast<std::size_t>(last + 1))) {
      ++last;
    }
    runs.push_back(last == tier
                       ? std::string(core::kDroneTierNames[tier])
                       : std::format("{}-{}", core::kDroneTierNames[tier],
                                     core::kDroneTierNames[last]));
    tier = last;
  }
  return runs.empty() ? "none" : join(runs);
}

auto droneNames(const core::DroneSet& drones) -> std::vector<std::string_view> {
  std::vector<std::string_view> names;
  for (std::size_t tier = 0; tier < drones.size(); ++tier) {
    if (drones.test(tier)) names.push_back(core::kDroneTierNames[tier]);
  }
  return names;
}

auto formatOutputs(const core::Asteroid& asteroid) -> std::vector<std::string> {
  const std::vector<double> shares = core::outputShares(asteroid.outputs);
  std::vector<std::string> out;
  for (std::size_t i = 0; i < asteroid.outputs.size(); ++i) {
    const core::Output& o = asteroid.outputs[i];
    std::string kind;
    if (o.prefix) {
      kind = o.prefix->starts_with("ore") ? " ore" : " " + *o.prefix;
    }
    out.push_back(std::format("{}{} x{} {:.1f}%", o.name, kind, o.amount,
                              100.0 * shares[i]));
  }
  return out;
}

auto whyUnminable(const core::Asteroid& asteroid,
                  const core::MinerModule& miner,
                  const std::optional<core::DroneSet>& drones)
    -> std::optional<std::string> {
  const auto needsModule = [&] {
    return std::format("needs a T{} module", asteroid.minModuleTier);
  };
  if (!drones) {
    if (core::fitsModule(asteroid, miner)) return std::nullopt;
    return needsModule();
  }
  const auto mined = core::canMine(asteroid, miner, *drones);
  if (mined) return std::nullopt;
  switch (mined.error()) {
    case core::Unminable::moduleTier:
      return needsModule();
    case core::Unminable::noDrone:
      return std::format("takes {} drones", formatDroneRange(asteroid));
    case core::Unminable::voltage:
      return std::format("needs over {} EU/t with these drones",
                         core::moduleVoltage(miner));
  }
  std::unreachable();
}

auto describe(core::DistanceError error, const core::Catalog& catalog,
              std::span<const std::size_t> targets,
              const core::MinerModule& miner,
              const std::optional<core::DroneSet>& drones) -> std::string {
  switch (error) {
    case core::DistanceError::noTargets:
      return "no target asteroid given";
    case core::DistanceError::noCommonRange:
      return std::format(
          "no distance reaches all the targets: {}",
          join(targets | std::views::transform([&](std::size_t i) {
                 const core::Asteroid& a = catalog.asteroids[i];
                 return std::format("{} {}", a.name, formatDistances(a));
               })));
    case core::DistanceError::notMinable: {
      std::vector<std::string> reasons;
      for (const std::size_t i : targets) {
        const core::Asteroid& a = catalog.asteroids[i];
        if (const auto why = whyUnminable(a, miner, drones)) {
          reasons.push_back(std::format("{} {}", a.name, *why));
        }
      }
      return std::format("a T{} module{} cannot mine: {}", miner.tier,
                         drones ? " with drones " + formatDrones(*drones) : "",
                         join(reasons));
    }
  }
  std::unreachable();
}

}  // namespace report
