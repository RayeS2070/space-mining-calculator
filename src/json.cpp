#include "json.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "core/distance.hpp"
#include "core/mechanics.hpp"
#include "core/model.hpp"
#include "report/report.hpp"

namespace cli::json {

namespace bj = boost::json;

namespace {

auto range(int min, int max) -> bj::object {
  return {{"min", min}, {"max", max}};
}

}  // namespace

auto asteroid(const core::Asteroid& a) -> bj::value {
  const std::vector<double> shares = core::outputShares(a.outputs);
  bj::array outputs;
  for (std::size_t i = 0; i < a.outputs.size(); ++i) {
    const core::Output& o = a.outputs[i];
    outputs.push_back(bj::object{
        {"name", o.name},
        {"prefix", o.prefix ? bj::value(*o.prefix) : bj::value(nullptr)},
        {"amount", o.amount},
        {"chance", o.chance},
        {"share", shares[i]},
    });
  }
  return bj::object{
      {"name", a.name},
      {"distance", range(a.minDistance, a.maxDistance)},
      {"drones",
       {{"first", core::kDroneTierNames[a.firstDroneTier]},
        {"last", core::kDroneTierNames[a.lastDroneTier]}}},
      {"moduleTier", a.minModuleTier},
      {"weight", a.weight},
      {"size", range(a.minSize, a.maxSize)},
      {"ticks", a.duration},
      {"eut", a.eut},
      {"computation", a.computation},
      {"requiresMod",
       a.requiresMod ? bj::value(*a.requiresMod) : bj::value(nullptr)},
      {"outputs", std::move(outputs)},
  };
}

auto asteroids(const core::Catalog& catalog, std::span<const std::size_t> rows)
    -> bj::value {
  bj::array out;
  for (const std::size_t i : rows)
    out.push_back(asteroid(catalog.asteroids[i]));
  return out;
}

auto bestDistances(std::span<const core::DistanceRange> distances,
                   const core::DroneSet& commonDrones,
                   const core::MinerModule& miner,
                   const std::optional<core::DroneSet>& given) -> bj::value {
  bj::array ranges;
  for (const core::DistanceRange& r : distances) {
    ranges.push_back(range(r.min, r.max));
  }
  return bj::object{
      {"distances", std::move(ranges)},
      {"commonDrones", drones(commonDrones)},
      {"moduleTier", miner.tier},
      {"drones", given ? drones(*given) : bj::value(nullptr)},
  };
}

auto drones(const core::DroneSet& set) -> bj::value {
  bj::array names;
  for (const std::string_view name : report::droneNames(set)) {
    names.emplace_back(name);
  }
  return names;
}

auto summary(const core::Catalog& catalog) -> bj::value {
  return bj::object{
      {"asteroids", catalog.asteroids.size()},
      {"recipes", core::expand(catalog.asteroids).size()},
      {"gt5u", catalog.gt5uVersion},
  };
}

auto error(std::string_view message) -> bj::value {
  return bj::object{{"error", message}};
}

}  // namespace cli::json
