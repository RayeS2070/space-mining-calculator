#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
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
#include <vector>

#include "core/distance.hpp"
#include "core/mechanics.hpp"
#include "core/model.hpp"
#include "json.hpp"
#include "reader/reader.hpp"
#include "report/report.hpp"

namespace po = boost::program_options;

namespace {

// Exit codes besides EXIT_SUCCESS.
constexpr int kFailure = 1;     // the command could not be done
constexpr int kBadOptions = 2;  // the command line is wrong

// Commands: at most one per run.
constexpr std::array<const char*, 5> kCommands = {
    "list", "show", "at", "best-distance", "drone-list"};

constexpr std::size_t kLineWidth = 100;
constexpr std::string_view kIndent = "   ";

// One table row, the name padded by padRight.
constexpr std::string_view kRowFormat =
    "{}  {:>8}  {:>7}  {:>6}  {:>6}  {:>7}  {:>5}  {:>8}  {}";

// Where the answer goes: text for people or JSON for programs.
enum class Format { text, json };

auto print(const boost::json::value& value) -> void {
  std::println("{}", boost::json::serialize(value));
}

// Reports an error and returns the exit code: with --json as {"error": ...}
// on stdout, the answer a program waits for; otherwise through spdlog.
auto fail(Format format, int code, std::string_view message) -> int {
  if (format == Format::json) {
    print(cli::json::error(message));
  } else {
    spdlog::error("{}", message);
  }
  return code;
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
               report::formatDistances(a), report::formatDroneRange(a),
               std::format("T{}", a.minModuleTier), a.weight,
               std::format("{}-{}", a.minSize, a.maxSize), a.duration, a.eut,
               a.requiresMod.value_or(""));

  // The outputs below the row, wrapped at kLineWidth.
  std::string line(kIndent);
  for (const std::string& output : report::formatOutputs(a)) {
    if (line.size() > kIndent.size() &&
        line.size() + 2 + output.size() > kLineWidth) {
      std::println("{}", line);
      line = kIndent;
    }
    line += line.size() > kIndent.size() ? ", " + output : " " + output;
  }
  std::println("{}", line);
}

// The asteroids at the indices: one row each with its outputs below, or a
// JSON array.
auto printAsteroids(const core::Catalog& catalog,
                    std::span<const std::size_t> rows, Format format) -> void {
  if (format == Format::json) {
    print(cli::json::asteroids(catalog, rows));
    return;
  }
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

auto list(const core::Catalog& catalog, Format format) -> int {
  const auto rows = std::views::iota(std::size_t{0}, catalog.asteroids.size()) |
                    std::ranges::to<std::vector>();
  printAsteroids(catalog, rows, format);
  return EXIT_SUCCESS;
}

auto show(const core::Catalog& catalog, std::span<const std::string> names,
          Format format) -> int {
  const auto rows = findAll(catalog, names);
  if (!rows) return fail(format, kFailure, rows.error());
  printAsteroids(catalog, *rows, format);
  return EXIT_SUCCESS;
}

// The asteroids at the distance that the module tier allows.
auto at(const core::Catalog& catalog, int distance,
        const core::MinerModule& miner, Format format) -> int {
  const auto rows =
      std::views::iota(std::size_t{0}, catalog.asteroids.size()) |
      std::views::filter([&](std::size_t i) {
        const core::Asteroid& a = catalog.asteroids[i];
        return core::fitsModule(a, miner) && core::isReachable(a, distance);
      }) |
      std::ranges::to<std::vector>();
  if (rows.empty() && format == Format::text) {
    std::println("No asteroid at distance {} for a T{} module.", distance,
                 miner.tier);
  } else {
    printAsteroids(catalog, rows, format);
  }
  return EXIT_SUCCESS;
}

// Without drones, the rough estimate of core::bestDistances.
auto bestDistance(const core::Catalog& catalog,
                  std::span<const std::string> names,
                  const core::MinerModule& miner,
                  const std::optional<core::DroneSet>& drones, Format format)
    -> int {
  const auto targets = findAll(catalog, names);
  if (!targets) return fail(format, kFailure, targets.error());
  const auto best =
      drones ? core::bestDistances(catalog.asteroids, *targets, miner,
                                   {.drones = *drones})
             : core::bestDistances(catalog.asteroids, *targets, miner);
  if (!best) {
    return fail(
        format, kFailure,
        report::describe(best.error(), catalog, *targets, miner, drones));
  }
  const core::DroneSet common =
      core::commonDroneTiers(catalog.asteroids, *targets);

  if (format == Format::json) {
    print(cli::json::bestDistances(*best, common, miner, drones));
    return EXIT_SUCCESS;
  }
  std::println(
      "distance: {}",
      report::join(*best | std::views::transform(report::formatRange)));
  if (common.any()) {
    std::println("drones for all the targets: {}",
                 report::formatDrones(common));
  } else {
    std::println("drones for all the targets: none, one drone each is needed");
  }
  return EXIT_SUCCESS;
}

auto droneList(Format format) -> int {
  if (format == Format::json) {
    print(cli::json::drones(core::DroneSet{}.set()));
    return EXIT_SUCCESS;
  }
  for (const std::string_view tier : core::kDroneTierNames) {
    std::println("{}", tier);
  }
  return EXIT_SUCCESS;
}

auto summary(const core::Catalog& catalog, Format format) -> int {
  if (format == Format::json) {
    print(cli::json::summary(catalog));
  } else {
    std::println("{} asteroids, {} recipes, GT5U {}. See --help for commands.",
                 catalog.asteroids.size(),
                 core::expand(catalog.asteroids).size(), catalog.gt5uVersion);
  }
  return EXIT_SUCCESS;
}

}  // namespace

auto main(int argc, char** argv) -> int {
  // Looked up before parsing, so a bad command line is answered in JSON too.
  const Format format =
      std::ranges::contains(
          std::span(argv, static_cast<std::size_t>(argc)) | std::views::drop(1),
          std::string_view("--json"))
          ? Format::json
          : Format::text;

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
    ("json", "answer in JSON, errors included ({\"error\": ...})")
    ("data", po::value<std::string>()->value_name("FILE"),
        "read the asteroids from the file instead of the built-in data")
    ("drones", po::value<std::vector<std::string>>()->multitoken()->value_name("TIER..."),
        "with --best-distance: the drones in the module, e.g. EV IV; without, "
        "a rough estimate that counts each asteroid once")
    ("module-tier", po::value<int>()->default_value(core::kModuleTierCount)->value_name("TIER"),
        "with --best-distance or --at: the mining module tier, 1 to 3");
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
    return fail(format, kBadOptions, std::format("{}; see --help", e.what()));
  }
  if (args.contains("help")) {
    std::ostringstream help;
    help << all;
    std::print("Usage: smc [--json] [--data FILE] [COMMAND]\n{}", help.str());
    return EXIT_SUCCESS;
  }
  if (std::ranges::count_if(
          kCommands, [&](const char* c) { return args.contains(c); }) > 1) {
    return fail(
        format, kBadOptions,
        std::format(
            "give one command at a time: {}",
            report::join(kCommands | std::views::transform([](const char* c) {
                           return std::format("--{}", c);
                         }))));
  }
  if (args.contains("drones") && !args.contains("best-distance")) {
    return fail(format, kBadOptions, "--drones goes with --best-distance");
  }
  if (!args["module-tier"].defaulted() && !args.contains("best-distance") &&
      !args.contains("at")) {
    return fail(format, kBadOptions,
                "--module-tier goes with --best-distance or --at");
  }
  const core::MinerModule miner{.tier = args["module-tier"].as<int>()};
  if (miner.tier < 1 || miner.tier > core::kModuleTierCount) {
    return fail(
        format, kBadOptions,
        std::format("--module-tier is 1 to {}", core::kModuleTierCount));
  }
  std::optional<core::DroneSet> drones;
  if (args.contains("drones")) {
    const auto parsed =
        parseDrones(args["drones"].as<std::vector<std::string>>());
    if (!parsed) return fail(format, kBadOptions, parsed.error());
    drones = *parsed;
  }
  if (args.contains("drone-list")) return droneList(format);

  const auto catalog = args.contains("data")
                           ? reader::readCatalog(args["data"].as<std::string>())
                           : reader::embeddedCatalog();
  if (!catalog) {
    return fail(format, kFailure,
                std::format("cannot load the asteroids: {}", catalog.error()));
  }

  if (args.contains("list")) return list(*catalog, format);
  if (args.contains("show")) {
    return show(*catalog, args["show"].as<std::vector<std::string>>(), format);
  }
  if (args.contains("at")) {
    return at(*catalog, args["at"].as<int>(), miner, format);
  }
  if (args.contains("best-distance")) {
    return bestDistance(*catalog,
                        args["best-distance"].as<std::vector<std::string>>(),
                        miner, drones, format);
  }
  return summary(*catalog, format);
}
