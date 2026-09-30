#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>

#include "cm/dtm.hpp"
#include "cm/retention.hpp"

using namespace cm;

namespace {
double plane(double x, double y) { return -1.5 + 0.2 * x + 0.1 * y; }

Cloud slopeCloud(bool with_stems, bool with_hole) {
  Cloud c;
  std::uint64_t s = 5;
  auto uni = [&] { s = splitmix64(s); return (s >> 11) * 0x1.0p-53; };
  for (int i = 0; i < 200000; ++i) {
    const double x = 20 * uni() - 10, y = 20 * uni() - 10;
    if (with_hole && std::hypot(x - 3, y + 3) < 1.2) continue;  // unscanned patch
    c.push_back(PointT(float(x), float(y), float(plane(x, y) + 0.003 * (uni() - 0.5))));
  }
  if (with_stems)  // vertical walls of points (no ground under part of them)
    for (int i = 0; i < 20000; ++i) {
      const double th = 2 * M_PI * uni(), h = 3 * uni();
      const double x = -2 + 0.3 * std::cos(th), y = 4 + 0.3 * std::sin(th);
      c.push_back(PointT(float(x), float(y), float(plane(x, y) + h)));  // wall starts on the ground
    }
  return c;
}
}  // namespace

double maxDtmError(const Dtm& d) {
  double maxerr = 0;
  for (double x = -9; x <= 9; x += 0.37)
    for (double y = -9; y <= 9; y += 0.41) {
      const auto h = d.height(x, y);
      if (!h) return 1e9;
      maxerr = std::max(maxerr, std::abs(*h - plane(x, y)));
    }
  return maxerr;
}

TEST(Dtm, UnbiasedOnSlopeWithHole) {
  // A naive "percentile height at cell centre" DTM would be ~4-5 cm low here.
  for (const char* mode : {"percentile", "supported_lowest"}) {
    DtmParams p;
    p.ground_select = mode;
    const Dtm d = Dtm::build(slopeCloud(false, true), p);
    EXPECT_LT(maxDtmError(d), 0.01) << mode;
    EXPECT_GT(d.stats().cells_filled, 0u);  // the hole was filled
  }
}

// Documents why supported_lowest is the default: with ground_select=percentile, cells where stem points
// (here ~5000) outnumber ground points (~125) push the 5th percentile ~8 cm up
// the stem; the 5x5 plane fit spreads that into a ~1.5 cm bump.
TEST(Dtm, DenseStemCellBiasesPercentile) {
  const Cloud c = slopeCloud(true, true);
  DtmParams pp;
  pp.ground_select = "percentile";
  const double e_pct = maxDtmError(Dtm::build(c, pp));
  DtmParams ps;
  ps.ground_select = "supported_lowest";
  const double e_sup = maxDtmError(Dtm::build(c, ps));
  EXPECT_EQ(DtmParams{}.ground_select, "supported_lowest");  // default
  RecordProperty("max_err_percentile_mm", int(e_pct * 1000));
  RecordProperty("max_err_supported_lowest_mm", int(e_sup * 1000));
  std::printf("[ info ] DTM max error near dense stem: percentile %.1f mm, supported_lowest %.1f mm\n", e_pct * 1e3,
              e_sup * 1e3);
  EXPECT_GT(e_pct, 0.01);   // the known bias is present with percentile
  EXPECT_LT(e_sup, 0.01);   // and absent with the alternative
}

TEST(Dtm, OutsideCoverageIsNullopt) {
  const Dtm d = Dtm::build(slopeCloud(false, false), DtmParams{});
  EXPECT_FALSE(d.height(50, 50).has_value());
  EXPECT_FALSE(d.height(-10.6, 0).has_value());
}

TEST(Dtm, HeightFilterKeepsZeroToThreeMetres) {
  Cloud c = slopeCloud(false, false);
  const Dtm d = Dtm::build(c, DtmParams{});
  Cloud q;
  for (double dz : {-0.2, -0.04, 0.0, 1.5, 2.99, 3.2}) q.push_back(PointT(1.f, 1.f, float(plane(1, 1) + dz)));
  const auto idx = heightFilter(q, d, HeightParams{});
  EXPECT_EQ(idx, (std::vector<int>{1, 2, 3, 4}));
}
