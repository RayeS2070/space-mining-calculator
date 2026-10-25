#include <spdlog/spdlog.h>

#include <algorithm>
#include <boost/program_options.hpp>
#include <cstddef>
#include <expected>
#include <format>
#include <limits>
#include <print>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

import reader;

namespace po = boost::program_options;

namespace {

constexpr std::size_t kLineWidth = 100;

// "Coal ore ×64 70.0%": the share is the game's exact one, see
// core::outputHits.
auto describeOutputs(const core::Asteroid& asteroid)
    -> std::vector<std::string> {
  const std::vector<int> hits = core::outputHits(asteroid.outputs);
  int total = 0;
  for (int h : hits) total += h;
  std::vector<std::string> out;
  for (std::size_t i = 0; i < asteroid.outputs.size(); ++i) {
    const core::Output& o = asteroid.outputs[i];
    std::string kind;
    if (o.prefix)
      kind = o.prefix->starts_with("ore") ? " ore" : " " + *o.prefix;
    out.push_back(std::format("{}{} x{} {:.1f}%", o.name, kind, o.amount,
                              100.0 * hits[i] / total));
  }
  return out;
}

auto printAsteroidInfo(const core::Asteroid& a, std::size_t maxNameWidth)
    -> void {
  std::println(
      "{:<{}}  {:>7}  {:>7}  {:>2}  {:>6}  {:>7}  {:>5}  {:>8}  {}", a.name,
      maxNameWidth, std::format("{}-{}", a.minDistance, a.maxDistance),
      std::format("{}-{}", core::kDroneTierNames[a.firstDroneTier],
                  core::kDroneTierNames[a.lastDroneTier]),
      a.minModuleTier, a.weight, std::format("{}-{}", a.minSize, a.maxSize),
      a.duration, a.eut, a.requiresMod.value_or(""));
  std::string line = "   ";
  for (const std::string& output : describeOutputs(a)) {
    if (line.size() > 3 && line.size() + 2 + output.size() > kLineWidth) {
      std::println("{}", line);
      line = "   ";
    }
    line += line.size() > 3 ? ", " + output : " " + output;
  }
  std::println("{}", line);
}

auto getMaxNameWidth(std::span<const core::Asteroid> asteroids) -> std::size_t {
  return std::ranges::max(asteroids, {},
                          [](const core::Asteroid& a) { return a.name.size(); })
      .name.size();
}

auto printHeader(const std::size_t nameWidth) -> void {
  std::println("{:<{}}  {:>7}  {:>7}  {:>2}  {:>6}  {:>7}  {:>5}  {:>8}  {}",
               "asteroid", nameWidth, "dist", "drones", "T", "weight", "size",
               "ticks", "EU/t", "needs");
}

// One row per asteroid, its outputs wrapped below it.
auto printAsteroids(std::span<const core::Asteroid> asteroids) -> void {
  const auto nameWidth = getMaxNameWidth(asteroids);
  printHeader(nameWidth);

  for (const core::Asteroid& a : asteroids) {
    printAsteroidInfo(a, nameWidth);
  }
}

auto findCommonRange(std::span<const core::Asteroid> asteroids)
    -> std::expected<std::pair<std::int32_t, std::int32_t>, std::string> {
  if (asteroids.empty()) return std::unexpected("empty range received");

  std::int32_t min = 0, max = std::numeric_limits<std::int32_t>::max();

  for (const auto& a : asteroids) {
    min = std::max(a.minDistance, min);
    max = std::min(a.maxDistance, max);
    if (max <= min) {
      return std::unexpected("no common range");
    }
  }

  return std::make_pair(min, max);
}

}  // namespace

auto main(int argc, char** argv) -> int {
  std::string dataPath;
  std::string infoTarget;
  std::int32_t distance;
  std::string asteroidsToDistanceFind;

  po::options_description options("Options");
  // clang-format off
  options.add_options()
    ("help,h", "show this help")
    ("data", po::value(&dataPath), "asteroid data instead of the built-in one, e.g. data/asteroids.json")
    ("list,l", "print list of asteroids")
    ("info,i", po::value(&infoTarget), "print info about target asteroid")
    ("distance,d", po::value(&distance), "print all asteroids on distance")
    ("find-distance-for", po::value(&asteroidsToDistanceFind), "");
  // clang-format on

  po::variables_map args;
  // program_options reports bad arguments by throwing: stopped here.
  try {
    po::store(po::parse_command_line(argc, argv, options), args);
    po::notify(args);
  } catch (const po::error& e) {
    spdlog::error("{}", e.what());
    return 1;
  }
  if (args.contains("help")) {
    std::ostringstream help;
    help << options;
    std::print("{}", help.str());
    return 0;
  }

  const auto catalog = args.contains("data") ? reader::readCatalog(dataPath)
                                             : reader::embeddedCatalog();

  if (!catalog) {
    spdlog::error("{}", catalog.error());
    return 1;
  }
  if (args.contains("list")) {
    printAsteroids(catalog->asteroids);
    return 0;
  }

  if (args.contains("info")) {
    const auto it = std::ranges::find(catalog->asteroids, infoTarget,
                                      &core::Asteroid::name);

    printHeader(it->name.size());
    printAsteroidInfo(*it, it->name.size());
    return 0;
  }

  if (args.contains("distance")) {
    auto asteroidsInDistance =
        catalog->asteroids |
        std::views::filter([distance](const core::Asteroid& a) {
          return distance >= a.minDistance && distance <= a.maxDistance;
        }) |
        std::ranges::to<std::vector<core::Asteroid>>();

    std::size_t nameWidth = getMaxNameWidth(asteroidsInDistance);
    printHeader(nameWidth);
    printAsteroids(asteroidsInDistance);
  }

  if (args.contains("find-distance-for")) {
    const auto values = asteroidsToDistanceFind | std::views::lazy_split(',') |
                        std::ranges::to<std::set<std::string>>();

    const auto targets = catalog->asteroids |
                         std::views::filter([&](const core::Asteroid& a) {
                           return values.contains(a.name);
                         }) |
                         std::ranges::to<std::vector<core::Asteroid>>();

    const auto rn = findCommonRange(targets);
    if (!rn) {
      spdlog::error("common range: {}", rn.error());
      return 1;
    }

    const auto [min, max] = *rn;
    std::int32_t solution = min;
    double maxFrac = 0.;
    const auto targetsWeight = std::ranges::fold_left(
        targets | std::views::transform(&core::Asteroid::weight), 0,
        std::plus<std::int32_t>{});

    for (const auto i : std::views::iota(min, max)) {
      auto range = catalog->asteroids | std::views::filter([i](const auto& a) {
                     return i >= a.minDistance && i <= a.maxDistance;
                   }) |
                   std::views::transform(&core::Asteroid::weight);

      const auto totalWeight =
          std::ranges::fold_left(range, 0, std::plus<std::int32_t>{});

      if (auto frac = static_cast<double>(targetsWeight) / totalWeight;
          frac >= maxFrac) {
        solution = i;
        maxFrac = frac;
      }
    }

    std::println("{}", solution);
    return 0;
  }

  std::println("{} asteroids, {} recipes (GT5U {})", catalog->asteroids.size(),
               core::expand(catalog->asteroids).size(), catalog->gt5uVersion);
  return 0;
}
