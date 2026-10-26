#include "core/distance.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <expected>
#include <initializer_list>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

#include "core/model.hpp"
#include "reader/reader.hpp"

namespace core {

// Found by gtest through ADL: failures show ranges, not bytes.
auto PrintTo(const DistanceRange& range, std::ostream* out) -> void {
  *out << range.min << ".." << range.max;
}

}  // namespace core

namespace {

using core::DistanceRange;
using Ranges = std::vector<DistanceRange>;
using Targets = std::vector<std::size_t>;

auto asteroid(int minDistance, int maxDistance, int weight, int duration = 100)
    -> core::Asteroid {
  // Assigned, not designated: clang warns of every field a designated
  // initializer leaves out.
  core::Asteroid a;
  a.minDistance = minDistance;
  a.maxDistance = maxDistance;
  a.duration = duration;
  a.weight = weight;
  return a;
}

auto withDrones(core::Asteroid a, int firstTier, int lastTier)
    -> core::Asteroid {
  a.firstDroneTier = firstTier;
  a.lastDroneTier = lastTier;
  return a;
}

auto drones(std::initializer_list<std::size_t> tiers) -> core::DroneSet {
  core::DroneSet set;
  for (const std::size_t tier : tiers) set.set(tier);
  return set;
}

constexpr core::MinerModule kMiner{.tier = core::kModuleTierCount};

TEST(FindAsteroid, ByName) {
  std::vector asteroids = {asteroid(1, 2, 1), asteroid(1, 2, 1)};
  asteroids[0].name = "a";
  asteroids[1].name = "b";
  EXPECT_EQ(core::findAsteroid(asteroids, "b"), 1u);
  EXPECT_EQ(core::findAsteroid(asteroids, "c"), std::nullopt);
}

TEST(IsReachable, IncludesBothBounds) {
  const core::Asteroid a = asteroid(5, 10, 1);
  EXPECT_FALSE(core::isReachable(a, 4));
  EXPECT_TRUE(core::isReachable(a, 5));
  EXPECT_TRUE(core::isReachable(a, 10));
  EXPECT_FALSE(core::isReachable(a, 11));
}

TEST(CommonDistanceRange, NoTargets) {
  const std::vector asteroids = {asteroid(1, 10, 1)};
  EXPECT_EQ(core::commonDistanceRange(asteroids, {}),
            std::unexpected(core::DistanceError::noTargets));
}

TEST(CommonDistanceRange, OneTarget) {
  const std::vector asteroids = {asteroid(5, 10, 1)};
  EXPECT_EQ(core::commonDistanceRange(asteroids, Targets{0}),
            (DistanceRange{.min = 5, .max = 10}));
}

TEST(CommonDistanceRange, Intersection) {
  const std::vector asteroids = {asteroid(1, 10, 1), asteroid(5, 20, 1)};
  EXPECT_EQ(core::commonDistanceRange(asteroids, Targets{0, 1}),
            (DistanceRange{.min = 5, .max = 10}));
}

// The ranges include both bounds, so they share 5.
TEST(CommonDistanceRange, OneSharedDistance) {
  const std::vector asteroids = {asteroid(1, 5, 1), asteroid(5, 10, 1)};
  EXPECT_EQ(core::commonDistanceRange(asteroids, Targets{0, 1}),
            (DistanceRange{.min = 5, .max = 5}));
}

TEST(CommonDistanceRange, Disjoint) {
  const std::vector asteroids = {asteroid(1, 4, 1), asteroid(5, 10, 1)};
  EXPECT_EQ(core::commonDistanceRange(asteroids, Targets{0, 1}),
            std::unexpected(core::DistanceError::noCommonRange));
}

TEST(BestDistances, Errors) {
  const std::vector asteroids = {asteroid(1, 4, 1), asteroid(5, 10, 1)};
  EXPECT_EQ(core::bestDistances(asteroids, {}),
            std::unexpected(core::DistanceError::noTargets));
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0, 1}),
            std::unexpected(core::DistanceError::noCommonRange));
}

TEST(BestDistances, AloneIsBest) {
  const std::vector asteroids = {asteroid(1, 10, 100), asteroid(1, 4, 100)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}),
            (Ranges{{.min = 5, .max = 10}}));
}

TEST(BestDistances, ReachesMaxDistance) {
  const std::vector asteroids = {asteroid(1, 10, 100), asteroid(1, 9, 100)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}),
            (Ranges{{.min = 10, .max = 10}}));
}

TEST(BestDistances, SplitPlateau) {
  const std::vector asteroids = {asteroid(1, 10, 100), asteroid(4, 6, 100)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}),
            (Ranges{{.min = 1, .max = 3}, {.min = 7, .max = 10}}));
}

// Weight alone favours 6..10 (100 of 150 against 100 of 200), but there the
// competitor is slow: Σ w·t is 100·100 + 100·10 = 11000 at 1..5 and
// 100·100 + 50·1000 = 60000 at 6..10.
TEST(BestDistances, ByTimeNotWeight) {
  const std::vector asteroids = {asteroid(1, 10, 100, 100),
                                 asteroid(1, 5, 100, 10),
                                 asteroid(6, 10, 50, 1000)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}),
            (Ranges{{.min = 1, .max = 5}}));
}

TEST(BestDistances, RepeatedTarget) {
  const std::vector asteroids = {asteroid(1, 10, 100), asteroid(1, 4, 100)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0, 0}),
            core::bestDistances(asteroids, Targets{0}));
}

TEST(CommonDroneTiers, Overlap) {
  const std::vector asteroids = {withDrones(asteroid(1, 10, 1), 0, 5),
                                 withDrones(asteroid(1, 10, 1), 3, 8)};
  EXPECT_EQ(core::commonDroneTiers(asteroids, Targets{0, 1}),
            drones({3, 4, 5}));
}

TEST(CommonDroneTiers, Disjoint) {
  const std::vector asteroids = {withDrones(asteroid(1, 10, 1), 0, 2),
                                 withDrones(asteroid(1, 10, 1), 3, 5)};
  EXPECT_TRUE(core::commonDroneTiers(asteroids, Targets{0, 1}).none());
}

TEST(IsMinable, NeedsDroneInRange) {
  const core::Asteroid a = withDrones(asteroid(1, 10, 1), 2, 4);
  EXPECT_FALSE(core::isMinable(a, kMiner, drones({1, 5})));
  EXPECT_TRUE(core::isMinable(a, kMiner, drones({3})));
}

TEST(IsMinable, NeedsModuleTier) {
  core::Asteroid a = asteroid(1, 10, 1);
  a.minModuleTier = 2;
  EXPECT_FALSE(core::isMinable(a, {.tier = 1}, drones({0})));
  EXPECT_TRUE(core::isMinable(a, {.tier = 2}, drones({0})));
}

TEST(BestDistancesWithDrones, NotMinable) {
  const std::vector asteroids = {withDrones(asteroid(1, 10, 1), 2, 4)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}, kMiner,
                                {.drones = drones({1})}),
            std::unexpected(core::DistanceError::notMinable));
}

// The competitor takes only the second tier: without that drone it is never
// picked, and every distance of the target is as good.
TEST(BestDistancesWithDrones, DroneLeavesCompetitorOut) {
  const std::vector asteroids = {withDrones(asteroid(1, 10, 100), 0, 1),
                                 withDrones(asteroid(1, 4, 100), 1, 1)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}, kMiner,
                                {.drones = drones({0})}),
            (Ranges{{.min = 1, .max = 10}}));
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}, kMiner,
                                {.drones = drones({0, 1})}),
            (Ranges{{.min = 5, .max = 10}}));
}

// An asteroid that two given drones reach is picked twice. Σ w·t at 1..5:
// target 100·100, the competitor 100·100 with the first drone and
// 100·⌈100/√2⌉ = 7100 with the second, 27100 in all; at 6..10:
// 100·100 + 150·100 = 25000. With the first drone alone 1..5 has 20000.
TEST(BestDistancesWithDrones, AsteroidCountsPerDrone) {
  const std::vector asteroids = {withDrones(asteroid(1, 10, 100), 0, 0),
                                 withDrones(asteroid(1, 5, 100), 0, 1),
                                 withDrones(asteroid(6, 10, 150), 0, 0)};
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}, kMiner,
                                {.drones = drones({0, 1})}),
            (Ranges{{.min = 6, .max = 10}}));
  EXPECT_EQ(core::bestDistances(asteroids, Targets{0}, kMiner,
                                {.drones = drones({0})}),
            (Ranges{{.min = 1, .max = 5}}));
}

class EmbeddedCatalog : public testing::Test {
 protected:
  static auto catalog() -> const core::Catalog& {
    static const core::Catalog c = reader::embeddedCatalog().value();
    return c;
  }
  static auto best(std::string_view name) -> std::optional<Ranges> {
    const auto i = core::findAsteroid(catalog().asteroids, name);
    if (!i) return std::nullopt;
    const auto ranges = core::bestDistances(catalog().asteroids, Targets{*i});
    if (!ranges) return std::nullopt;
    return *ranges;
  }
};

TEST_F(EmbeddedCatalog, Loads) { EXPECT_FALSE(catalog().asteroids.empty()); }

// The answers below follow data/asteroids.json: regenerating it may change
// them.

// Reachable at 1..250. By weight 241..250 would win; there the asteroids
// weigh less in sum but take longer.
TEST_F(EmbeddedCatalog, Salt) {
  EXPECT_EQ(best("saltAsteroid"), (Ranges{{.min = 1, .max = 1}}));
}

TEST_F(EmbeddedCatalog, Magnesium) {
  EXPECT_EQ(best("magnesiumAsteroid"), (Ranges{{.min = 13, .max = 16}}));
}

}  // namespace
