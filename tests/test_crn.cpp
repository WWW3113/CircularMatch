// Common random numbers: order independence, subset property, empirical rate.
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <tuple>

#include "cm/retention.hpp"

using namespace cm;

namespace {
Cloud randomCloud(std::size_t n, std::uint64_t s) {
  Cloud c;
  for (std::size_t i = 0; i < n; ++i) {
    s = splitmix64(s);
    const double a = (s >> 11) * 0x1.0p-53;
    s = splitmix64(s);
    const double b = (s >> 11) * 0x1.0p-53;
    s = splitmix64(s);
    const double z = (s >> 11) * 0x1.0p-53;
    c.push_back(PointT(float(40 * a - 20), float(40 * b - 20), float(3 * z)));
  }
  return c;
}
using Key = std::tuple<float, float, float>;
std::set<Key> keptSet(const Cloud& c, const RetentionModel& m, std::uint64_t seed) {
  std::set<Key> s;
  for (const auto& p : c)
    if (commonUniform(seed, p, 1e-4) < m(scannerDistance(p, false))) s.insert({p.x, p.y, p.z});
  return s;
}
}  // namespace

TEST(CRN, UniformInUnitInterval) {
  const Cloud c = randomCloud(20000, 7);
  for (const auto& p : c) {
    const double u = commonUniform(3, p, 1e-4);
    ASSERT_GE(u, 0.0);
    ASSERT_LT(u, 1.0);
  }
}

TEST(CRN, IndependentOfProcessingOrder) {
  Cloud c = randomCloud(20000, 11);
  const auto m = RetentionModel::make(RetentionKind::Step, RetentionParams{});
  const auto a = keptSet(c, m, 5);
  std::reverse(c.points.begin(), c.points.end());
  std::rotate(c.points.begin(), c.points.begin() + 1234, c.points.end());
  EXPECT_EQ(a, keptSet(c, m, 5));
}

TEST(CRN, DifferentSeedsGiveDifferentDraws) {
  const Cloud c = randomCloud(20000, 13);
  const auto m = RetentionModel::make(RetentionKind::Step, RetentionParams{});
  EXPECT_NE(keptSet(c, m, 1), keptSet(c, m, 2));
}

TEST(CRN, SubsetWhenPointwiseSmaller) {
  const Cloud c = randomCloud(50000, 17);
  RetentionParams rp;
  const auto step = RetentionModel::make(RetentionKind::Step, rp);
  const auto la = RetentionModel::make(RetentionKind::LinearA, rp);
  const auto lm = RetentionModel::make(RetentionKind::LinearMid, rp);
  for (std::uint64_t seed = 1; seed <= 5; ++seed) {
    const auto s_step = keptSet(c, step, seed), s_la = keptSet(c, la, seed), s_lm = keptSet(c, lm, seed);
    EXPECT_TRUE(std::includes(s_la.begin(), s_la.end(), s_step.begin(), s_step.end())) << "step ⊄ linear_a";
    EXPECT_TRUE(std::includes(s_la.begin(), s_la.end(), s_lm.begin(), s_lm.end())) << "linear_mid ⊄ linear_a";
    EXPECT_LT(s_step.size(), s_la.size());
  }
}

TEST(CRN, EmpiricalRateMatchesP) {
  const Cloud c = randomCloud(200000, 19);
  for (auto k : {RetentionKind::Step, RetentionKind::LinearA, RetentionKind::LinearMid, RetentionKind::Physical}) {
    const auto m = RetentionModel::make(k, RetentionParams{});
    std::vector<double> d;
    std::vector<char> kept;
    for (const auto& p : c) {
      d.push_back(scannerDistance(p, false));
      kept.push_back(commonUniform(9, p, 1e-4) < m(d.back()));
    }
    for (const auto& b : retentionBins(d, kept, m, 2.0)) {
      if (b.n < 2000) continue;
      const double rate = double(b.kept) / double(b.n);
      const double sd = std::sqrt(b.p_mean * (1 - b.p_mean) / double(b.n));
      EXPECT_NEAR(rate, b.p_mean, 5 * sd + 1e-12) << toString(k) << " bin " << b.d_lo;
    }
  }
}
