#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <boost/program_options.hpp>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <format>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/distance.hpp"
#include "core/mechanics.hpp"
#include "core/model.hpp"
#include "reader/reader.hpp"

namespace po = boost::program_options;

namespace {

// Exit codes besides EXIT_SUCCESS.
constexpr int kFailure = 1;     // the command could not be done
constexpr int kBadOptions = 2;  // the command line is wrong

// Commands: at most one per run.
constexpr std::array<const char*, 5> kCommands = {
    "list", "show", "at", "best-distance", "drone-list"};

// No option for the module tier yet: T3 filters the fewest recipes, in the
// current data none but by the drones.
constexpr core::MinerModule kMiner{.tier = core::kModuleTierCount};

constexpr std::size_t kLineWidth = 100;
constexpr std::string_view kIndent = "   ";

// One table row, the name padded by padRight.
constexpr std::string_view kRowFormat =
    "{}  {:>8}  {:>7}  {:>6}  {:>6}  {:>7}  {:>5}  {:>8}  {}";

// "Coal ore x64 70.0%": the share is the game's exact one, see
// core::outputHits.
auto formatOutputs(const core::Asteroid& asteroid) -> std::vector<std::string> {
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

// "a, b, c".
auto join(std::ranges::input_range auto&& parts) -> std::string {
  return parts | std::views::join_with(std::string_view(", ")) |
         std::ranges::to<std::string>();
}

auto formatRange(const core::DistanceRange& range) -> std::string {
  return range.min == range.max ? std::format("{}", range.min)
                                : std::format("{}-{}", range.min, range.max);
}

// Runs of adjacent tiers: "LV-IV, UV"; "none" for an empty set.
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

auto equalsIgnoringCase(std::string_view a, std::string_view b) -> bool {
  return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) {
    return std::tolower(x) == std::tolower(y);
  });
}

// The named drone tiers, in any letter case. The error names every unknown
// one.
auto parseDrones(std::span<const std::string> names)
    -> std::expected<core::DroneSet, std::string> {
  core::DroneSet drones;
  std::vector<std::string_view> unknown;
  for (const std::string& name : names) {
    const auto it = std::ranges::find_if(
        core::kDroneTierNames,
        [&](std::string_view tier) { return equalsIgnoringCase(tier, name); });
    if (it == core::kDroneTierNames.end()) {
      unknown.push_back(name);
    } else {
      drones.set(static_cast<std::size_t>(it - core::kDroneTierNames.begin()));
    }
  }
  if (!unknown.empty()) {
    return std::unexpected(
        std::format("unknown drone tier{} {:n}; see --drone-list",
                    unknown.size() > 1 ? "s" : "", unknown));
  }
  return drones;
}

auto formatDroneRange(const core::Asteroid& a) -> std::string {
  return std::format("{}-{}", core::kDroneTierNames[a.firstDroneTier],
                     core::kDroneTierNames[a.lastDroneTier]);
}

// The text left-aligned in the width. Not "{:<{}}": clang 23 with libstdc++ 16
// rejects every dynamic width in C++26, as the definition of
// basic_format_parse_context::__check_dynamic_spec comes after its use.
auto padRight(std::string_view text, std::size_t width) -> std::string {
  std::string padded(text);
  padded.resize(std::max(width, text.size()), ' ');
  return padded;
}

auto printRow(const core::Asteroid& a, std::size_t nameWidth) -> void {
  std::println(kRowFormat, padRight(a.name, nameWidth),
               formatRange({.min = a.minDistance, .max = a.maxDistance}),
               formatDroneRange(a), std::format("T{}", a.minModuleTier),
               a.weight, std::format("{}-{}", a.minSize, a.maxSize), a.duration,
               a.eut, a.requiresMod.value_or(""));

  // The outputs below the row, wrapped at kLineWidth.
  std::string line(kIndent);
  for (const std::string& output : formatOutputs(a)) {
    if (line.size() > kIndent.size() &&
        line.size() + 2 + output.size() > kLineWidth) {
      std::println("{}", line);
      line = kIndent;
    }
    line += line.size() > kIndent.size() ? ", " + output : " " + output;
  }
  std::println("{}", line);
}

// The asteroids at the indices, one row each with its outputs below.
auto printTable(const core::Catalog& catalog, std::span<const std::size_t> rows)
    -> void {
  constexpr std::string_view kNameHeader = "asteroid";
  std::size_t nameWidth = kNameHeader.size();
  for (const std::size_t i : rows) {
    nameWidth = std::max(nameWidth, catalog.asteroids[i].name.size());
  }

  std::println(kRowFormat, padRight(kNameHeader, nameWidth), "distance",
               "drones", "module", "weight", "size", "ticks", "EU/t",
               "requires");
  for (const std::size_t i : rows) printRow(catalog.asteroids[i], nameWidth);
}

// Indices of the named asteroids. The error names every unknown one.
auto findAll(const core::Catalog& catalog, std::span<const std::string> names)
    -> std::expected<std::vector<std::size_t>, std::string> {
  std::vector<std::size_t> found;
  std::vector<std::string_view> unknown;
  for (const std::string& name : names) {
    if (const auto i = core::findAsteroid(catalog.asteroids, name)) {
      found.push_back(*i);
    } else {
      unknown.push_back(name);
    }
  }
  if (!unknown.empty()) {
    return std::unexpected(std::format("unknown asteroid{} {:n}; see --list",
                                       unknown.size() > 1 ? "s" : "", unknown));
  }
  return found;
}

auto describe(core::DistanceError error, const core::Catalog& catalog,
              std::span<const std::size_t> targets,
              const core::DroneSet& drones) -> std::string {
  switch (error) {
    case core::DistanceError::noTargets:
      return "no target asteroid given";
    case core::DistanceError::noCommonRange:
      return std::format(
          "no distance reaches all the targets: {}",
          join(targets | std::views::transform([&](std::size_t i) {
                 const core::Asteroid& a = catalog.asteroids[i];
                 return std::format(
                     "{} {}", a.name,
                     formatRange({.min = a.minDistance, .max = a.maxDistance}));
               })));
    case core::DistanceError::notMinable:
      return std::format("the drones {} mine none of: {}", formatDrones(drones),
                         join(targets | std::views::filter([&](std::size_t i) {
                                return !core::isMinable(catalog.asteroids[i],
                                                        kMiner, drones);
                              }) |
                              std::views::transform([&](std::size_t i) {
                                const core::Asteroid& a = catalog.asteroids[i];
                                return std::format("{} (takes {})", a.name,
                                                   formatDroneRange(a));
                              })));
  }
  std::unreachable();
}

auto list(const core::Catalog& catalog) -> int {
  const auto rows = std::views::iota(std::size_t{0}, catalog.asteroids.size()) |
                    std::ranges::to<std::vector>();
  printTable(catalog, rows);
  return EXIT_SUCCESS;
}

auto show(const core::Catalog& catalog, std::span<const std::string> names)
    -> int {
  const auto rows = findAll(catalog, names);
  if (!rows) {
    spdlog::error("{}", rows.error());
    return kFailure;
  }
  printTable(catalog, *rows);
  return EXIT_SUCCESS;
}

auto at(const core::Catalog& catalog, int distance) -> int {
  const auto rows = std::views::iota(std::size_t{0}, catalog.asteroids.size()) |
                    std::views::filter([&](std::size_t i) {
                      return core::isReachable(catalog.asteroids[i], distance);
                    }) |
                    std::ranges::to<std::vector>();
  if (rows.empty()) {
    std::println("No asteroid at distance {}.", distance);
  } else {
    printTable(catalog, rows);
  }
  return EXIT_SUCCESS;
}

// Without drones, the rough estimate of core::bestDistances.
auto bestDistance(const core::Catalog& catalog,
                  std::span<const std::string> names,
                  const std::optional<core::DroneSet>& drones) -> int {
  const auto targets = findAll(catalog, names);
  if (!targets) {
    spdlog::error("{}", targets.error());
    return kFailure;
  }
  const auto best = drones ? core::bestDistances(catalog.asteroids, *targets,
                                                 kMiner, {.drones = *drones})
                           : core::bestDistances(catalog.asteroids, *targets);
  if (!best) {
    spdlog::error("{}", describe(best.error(), catalog, *targets,
                                 drones.value_or(core::DroneSet{})));
    return kFailure;
  }
  std::println("distance: {}",
               join(*best | std::views::transform(formatRange)));

  const core::DroneSet common =
      core::commonDroneTiers(catalog.asteroids, *targets);
  if (common.any()) {
    std::println("drones for all the targets: {}", formatDrones(common));
  } else {
    std::println("drones for all the targets: none, one drone each is needed");
  }
  return EXIT_SUCCESS;
}

auto droneList() -> int {
  for (const std::string_view tier : core::kDroneTierNames) {
    std::println("{}", tier);
  }
  return EXIT_SUCCESS;
}

}  // namespace

auto main(int argc, char** argv) -> int {
  po::options_description commands(
      "Commands (one at a time; with none, a summary of the data)");
  // clang-format off
  commands.add_options()
    ("list,l", "list all asteroids")
    ("show,s", po::value<std::vector<std::string>>()->multitoken()->value_name("NAME..."),
        "show the named asteroids, e.g. coalAsteroid")
    ("at", po::value<int>()->value_name("DISTANCE"),
        "list the asteroids reachable at the distance")
    ("best-distance", po::value<std::vector<std::string>>()->multitoken()->value_name("NAME..."),
        "the distances where the named asteroids come up most often per tick")
    ("drone-list", "list the drone tiers");
  // clang-format on
  po::options_description options("Options");
  // clang-format off
  options.add_options()
    ("help,h", "show this help")
    ("data", po::value<std::string>()->value_name("FILE"),
        "read the asteroids from the file instead of the built-in data")
    ("drones", po::value<std::vector<std::string>>()->multitoken()->value_name("TIER..."),
        "with --best-distance: the drones in the module, e.g. EV IV; without, "
        "a rough estimate that counts each asteroid once");
  // clang-format on
  po::options_description all;
  all.add(commands).add(options);

  po::variables_map args;
  // program_options reports a bad command line by throwing: stopped here.
  try {
    // No positional arguments: an empty description makes a stray word an
    // error, parse_command_line would drop it silently.
    po::store(po::command_line_parser(argc, argv)
                  .options(all)
                  .positional(po::positional_options_description{})
                  .run(),
              args);
    po::notify(args);
  } catch (const po::error& e) {
    spdlog::error("{}; see --help", e.what());
    return kBadOptions;
  }
  if (args.contains("help")) {
    std::ostringstream help;
    help << all;
    std::print("Usage: smc [--data FILE] [COMMAND]\n{}", help.str());
    return EXIT_SUCCESS;
  }
  if (std::ranges::count_if(
          kCommands, [&](const char* c) { return args.contains(c); }) > 1) {
    spdlog::error("give one command at a time: {}",
                  join(kCommands | std::views::transform([](const char* c) {
                         return std::format("--{}", c);
                       })));
    return kBadOptions;
  }
  if (args.contains("drones") && !args.contains("best-distance")) {
    spdlog::error("--drones goes with --best-distance");
    return kBadOptions;
  }
  std::optional<core::DroneSet> drones;
  if (args.contains("drones")) {
    const auto parsed =
        parseDrones(args["drones"].as<std::vector<std::string>>());
    if (!parsed) {
      spdlog::error("{}", parsed.error());
      return kBadOptions;
    }
    drones = *parsed;
  }
  if (args.contains("drone-list")) return droneList();

  const auto catalog = args.contains("data")
                           ? reader::readCatalog(args["data"].as<std::string>())
                           : reader::embeddedCatalog();
  if (!catalog) {
    spdlog::error("cannot load the asteroids: {}", catalog.error());
    return kFailure;
  }

  if (args.contains("list")) return list(*catalog);
  if (args.contains("show")) {
    return show(*catalog, args["show"].as<std::vector<std::string>>());
  }
  if (args.contains("at")) return at(*catalog, args["at"].as<int>());
  if (args.contains("best-distance")) {
    return bestDistance(
        *catalog, args["best-distance"].as<std::vector<std::string>>(), drones);
  }

  std::println("{} asteroids, {} recipes, GT5U {}. See --help for commands.",
               catalog->asteroids.size(),
               core::expand(catalog->asteroids).size(), catalog->gt5uVersion);
  return EXIT_SUCCESS;
}
