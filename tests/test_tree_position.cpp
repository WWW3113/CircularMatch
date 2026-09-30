#include <gtest/gtest.h>

#include <cmath>

#include "cm/dtm.hpp"
#include "cm/retention.hpp"
#include "cm/tree_position.hpp"

using namespace cm;

namespace {
Dtm slopeDtm(double sx, double sy) {
  Cloud c;
  for (double x = -12; x <= 12; x += 0.05)
    for (double y = -12; y <= 12; y += 0.05) c.push_back(PointT(float(x), float(y), float(sx * x + sy * y)));
  return Dtm::build(c, DtmParams{});
}
}  // namespace

TEST(TreePosition, TiltedAxisOnSlopeMatchesAnalytic) {
  const double sx = 0.3, sy = -0.2;
  const Dtm d = slopeDtm(sx, sy);
  for (double tilt : {0.0, 10.0, 20.0, 30.0})
    for (double az : {0.0, 90.0, 180.0, 225.0}) {
      const double t = tilt * M_PI / 180, a = az * M_PI / 180;
      const Eigen::Vector3d v(std::sin(t) * std::cos(a), std::sin(t) * std::sin(a), std::cos(t));
      const Eigen::Vector3d base(2.0, -1.0, sx * 2.0 + sy * -1.0);
      const Eigen::Vector3d p = base + 1.7 * v;  // some point up the axis
      const auto r = axisDtmIntersection(p, v, d, IntersectParams{}, 0.0);
      ASSERT_TRUE(r.has_value()) << tilt << " " << az;
      EXPECT_LT((r->point - base).norm(), 0.003) << "tilt " << tilt << " az " << az;
      // Using the DTM height at (p_x, p_y) would be wrong for a tilted axis:
      if (tilt > 0) {
        const Eigen::Vector3d naive(p.x(), p.y(), *d.height(p.x(), p.y()));
        EXPECT_GT((naive.head<2>() - base.head<2>()).norm(), 0.1);
      }
    }
}

TEST(TreePosition, SteepCaseFallsBackToBisection) {
  // Terrain slope 1.5 and axis tilted 45 deg towards uphill: fixed point diverges.
  const Dtm d = slopeDtm(1.5, 0.0);
  const double t = 45 * M_PI / 180;
  const Eigen::Vector3d v(std::sin(t), 0, std::cos(t));
  const Eigen::Vector3d base(1.0, 0.0, 1.5);
  const auto r = axisDtmIntersection(base + 1.0 * v, v, d, IntersectParams{}, 0.0);
  ASSERT_TRUE(r.has_value());
  EXPECT_TRUE(r->used_bisection);
  EXPECT_LT((r->point - base).norm(), 0.005);
}

TEST(TreePosition, OutsideDtmReturnsNullopt) {
  const Dtm d = slopeDtm(0.0, 0.0);
  const auto r = axisDtmIntersection({100, 100, 1}, {0, 0, 1}, d, IntersectParams{}, 0.0);
  EXPECT_FALSE(r.has_value());
}
