#include "core/mechanics.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "core/model.hpp"

namespace core {
namespace {

auto chances(std::initializer_list<int> list) -> std::vector<Output> {
  std::vector<Output> out;
  for (const int c : list)
    out.push_back({.name = "x", .prefix = {}, .amount = 1, .chance = c});
  return out;
}

// GT5U 5.09.54.264: random -= chance_j until it goes below 0, so each output
// takes exactly its chance out of the sum.
TEST(OutputHits, EachOutputItsChance) {
  EXPECT_EQ(outputHits(chances({7000, 1000, 2000})),
            (std::vector<int>{7000, 1000, 2000}));
}

TEST(OutputHits, SumNot10000) {
  EXPECT_EQ(outputHits(chances({4000, 3000, 2000})),
            (std::vector<int>{4000, 3000, 2000}));
}

TEST(OutputHits, ZeroChanceNeverHit) {
  EXPECT_EQ(outputHits(chances({0, 5000, 0})), (std::vector<int>{0, 5000, 0}));
}

TEST(OutputShares, ChanceOverSum) {
  const auto shares = outputShares(chances({7000, 3000}));
  ASSERT_EQ(shares.size(), 2);
  EXPECT_DOUBLE_EQ(shares[0], 0.7);
  EXPECT_DOUBLE_EQ(shares[1], 0.3);
}

}  // namespace
}  // namespace core
