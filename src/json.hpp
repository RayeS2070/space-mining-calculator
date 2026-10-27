#ifndef SMC_SRC_JSON_HPP
#define SMC_SRC_JSON_HPP

#include <boost/json/value.hpp>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/distance.hpp"
#include "core/model.hpp"

// The --json answers of the command line tool. Values are plain numbers and
// names, so a reader needs no knowledge of the text output.

namespace cli::json {

// The asteroid as data/asteroids.json has it, with each output's exact
// share.
auto asteroid(const core::Asteroid& asteroid) -> boost::json::value;

// The asteroids at the indices.
auto asteroids(const core::Catalog& catalog, std::span<const std::size_t> rows)
    -> boost::json::value;

// {"distances": [{"min", "max"}...], "commonDrones": [...],
//  "moduleTier": n, "drones": [...] or null for the rough estimate}.
auto bestDistances(std::span<const core::DistanceRange> distances,
                   const core::DroneSet& commonDrones,
                   const core::MinerModule& miner,
                   const std::optional<core::DroneSet>& drones)
    -> boost::json::value;

// The tier names.
auto drones(const core::DroneSet& drones) -> boost::json::value;

auto summary(const core::Catalog& catalog) -> boost::json::value;

// {"error": message}.
auto error(std::string_view message) -> boost::json::value;

}  // namespace cli::json

#endif  // SMC_SRC_JSON_HPP
