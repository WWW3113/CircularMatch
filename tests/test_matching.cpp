#include <gtest/gtest.h>

#include "cm/matching.hpp"

using namespace cm;

TEST(Matching, OptimalBeatsGreedy) {
  // Greedy (closest pair first) would match det0-ref1 (0.1) and leave det1 unmatched
  // (det1-ref0 is 0.6 > radius). Optimal: det0-ref0 (0.3) + det1-ref1 (0.3) -> 2 pairs.
  const std::vector<Eigen::Vector2d> det = {{0.0, 0.0}, {0.4, 0.0}};
  const std::vector<Eigen::Vector2d> ref = {{-0.3, 0.0}, {0.1, 0.0}};
  const auto m = matchPositions(det, ref, 0.5);
  ASSERT_EQ(m.pairs.size(), 2u);
  EXPECT_NEAR(m.errors[0] + m.errors[1], 0.3 + 0.3, 1e-12);
  EXPECT_DOUBLE_EQ(m.recall(), 1.0);
  EXPECT_DOUBLE_EQ(m.precision(), 1.0);
}

TEST(Matching, MinimumTotalDistanceAmongMaximumMatchings) {
  const std::vector<Eigen::Vector2d> det = {{0, 0}, {1, 0}};
  const std::vector<Eigen::Vector2d> ref = {{0.1, 0}, {0.9, 0}};
  const auto m = matchPositions(det, ref, 2.0);  // both assignments admissible
  ASSERT_EQ(m.pairs.size(), 2u);
  EXPECT_NEAR(m.errors[0] + m.errors[1], 0.2, 1e-12);
}

TEST(Matching, RadiusRespectedAndOrderIndependent) {
  std::vector<Eigen::Vector2d> det = {{0, 0}, {5, 5}, {10, 0.2}, {3, 3}};
  std::vector<Eigen::Vector2d> ref = {{0.2, 0}, {10, 0}, {20, 20}};
  const auto a = matchPositions(det, ref, 0.5);
  EXPECT_EQ(a.pairs.size(), 2u);
  EXPECT_NEAR(a.recall(), 2.0 / 3.0, 1e-12);
  EXPECT_NEAR(a.precision(), 0.5, 1e-12);
  std::reverse(det.begin(), det.end());
  std::reverse(ref.begin(), ref.end());
  const auto b = matchPositions(det, ref, 0.5);
  EXPECT_EQ(b.pairs.size(), a.pairs.size());
  EXPECT_NEAR(b.mean_error(), a.mean_error(), 1e-12);
}

TEST(Matching, EmptyInputs) {
  EXPECT_EQ(matchPositions({}, {{0, 0}}, 0.5).pairs.size(), 0u);
  EXPECT_EQ(matchPositions({{0, 0}}, {}, 0.5).precision(), 0.0);
}
