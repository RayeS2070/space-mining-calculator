#include "report/report.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/distance.hpp"
#include "core/model.hpp"

namespace {

auto drones(std::initializer_list<std::size_t> tiers) -> core::DroneSet {
  core::DroneSet set;
  for (const std::size_t tier : tiers) set.set(tier);
  return set;
}

auto catalogOf(std::vector<core::Asteroid> asteroids) -> core::Catalog {
  return {.gt5uVersion = "test", .asteroids = std::move(asteroids)};
}

TEST(FormatRange, OneOrMany) {
  EXPECT_EQ(report::formatRange({.min = 5, .max = 5}), "5");
  EXPECT_EQ(report::formatRange({.min = 1, .max = 40}), "1-40");
}

TEST(FormatDrones, Runs) {
  EXPECT_EQ(report::formatDrones({}), "none");
  EXPECT_EQ(report::formatDrones(drones({0, 1, 2, 3, 4, 7})), "LV-IV, UV");
  EXPECT_EQ(report::formatDrones(drones({13})), "MAX");
}

TEST(FormatOutputs, ExactShares) {
  core::Asteroid a;
  a.outputs = {
      {.name = "Coal", .prefix = "oreEndstone", .amount = 64, .chance = 7000},
      {.name = "Clay", .prefix = std::nullopt, .amount = 1, .chance = 3000}};
  EXPECT_EQ(report::formatOutputs(a),
            (std::vector<std::string>{"Coal ore x64 70.0%", "Clay x1 30.0%"}));
}

TEST(WhyUnminable, Reasons) {
  core::Asteroid a;
  a.minDistance = 1;
  a.maxDistance = 10;
  a.firstDroneTier = 2;
  a.lastDroneTier = 4;
  a.minModuleTier = 2;
  EXPECT_EQ(report::whyUnminable(a, {.tier = 1}, std::nullopt),
            "needs a T2 module");
  EXPECT_EQ(report::whyUnminable(a, {.tier = 2}, std::nullopt), std::nullopt);
  EXPECT_EQ(report::whyUnminable(a, {.tier = 2}, drones({0})),
            "takes HV-IV drones");
  EXPECT_EQ(report::whyUnminable(a, {.tier = 2}, drones({3})), std::nullopt);
}

TEST(Describe, NotMinableNamesEachTarget) {
  core::Asteroid a;
  a.name = "a";
  a.maxDistance = 10;
  a.minModuleTier = 3;
  core::Asteroid b = a;
  b.name = "b";
  b.minModuleTier = 1;
  const core::Catalog catalog = catalogOf({a, b});
  const std::vector<std::size_t> targets = {0, 1};
  EXPECT_EQ(report::describe(core::DistanceError::notMinable, catalog, targets,
                             {.tier = 1}, drones({0})),
            "a T1 module with drones LV cannot mine: a needs a T3 module");
}

}  // namespace
