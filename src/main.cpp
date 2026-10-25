#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <boost/program_options.hpp>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <format>
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
constexpr std::array<const char*, 4> kCommands = {"list", "show", "at",
                                                  "best-distance"};

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
               std::format("{}-{}", core::kDroneTierNames[a.firstDroneTier],
                           core::kDroneTierNames[a.lastDroneTier]),
               std::format("T{}", a.minModuleTier), a.weight,
               std::format("{}-{}", a.minSize, a.maxSize), a.duration, a.eut,
               a.requiresMod.value_or(""));

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
              std::span<const std::size_t> targets) -> std::string {
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

auto bestDistance(const core::Catalog& catalog,
                  std::span<const std::string> names) -> int {
  const auto targets = findAll(catalog, names);

  if (!targets) {
    spdlog::error("{}", targets.error());
    return kFailure;
  }
  const auto best = core::bestDistances(catalog.asteroids, *targets);
  if (!best) {
    spdlog::error("{}", describe(best.error(), catalog, *targets));
    return kFailure;
  }
  std::println("{}", join(*best | std::views::transform(formatRange)));
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
        "the distances where the named asteroids come up most often per tick");
  // clang-format on
  po::options_description options("Options");
  // clang-format off
  options.add_options()
    ("help,h", "show this help")
    ("data", po::value<std::string>()->value_name("FILE"),
        "read the asteroids from the file instead of the built-in data");
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
    return bestDistance(*catalog,
                        args["best-distance"].as<std::vector<std::string>>());
  }

  std::println("{} asteroids, {} recipes, GT5U {}. See --help for commands.",
               catalog->asteroids.size(),
               core::expand(catalog->asteroids).size(), catalog->gt5uVersion);
  return EXIT_SUCCESS;
}
