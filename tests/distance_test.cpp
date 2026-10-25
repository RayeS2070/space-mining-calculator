#include <gtest/gtest.h>

#include <cstddef>
#include <expected>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

import reader;

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
