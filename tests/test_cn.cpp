// CN descriptor, matching (Algorithm 1) and registration tests.
#include <gtest/gtest.h>

#include <Eigen/Geometry>
#include <cmath>

#include "cm/cn_descriptor.hpp"
#include "cm/cn_matching.hpp"
#include "cm/registration.hpp"
#include "cm/retention.hpp"  // splitmix64

using namespace cm;

namespace {
struct Rng {
  std::uint64_t s;
  double uni() {
    s = splitmix64(s);
    return (s >> 11) * 0x1.0p-53;
  }
  double normal() { return std::sqrt(-2 * std::log(std::max(uni(), 1e-300))) * std::cos(2 * M_PI * uni()); }
};

// Random "forest": n stems in a disk of radius r with minimum spacing, base heights on a slope.
std::vector<Eigen::Vector3d> forest(int n, double r, double min_spacing, std::uint64_t seed) {
  Rng g{seed};
  std::vector<Eigen::Vector3d> out;
  while (static_cast<int>(out.size()) < n) {
    const double x = (2 * g.uni() - 1) * r, y = (2 * g.uni() - 1) * r;
    if (std::hypot(x, y) > r) continue;
    bool ok = true;
    for (const auto& q : out) ok &= std::hypot(q.x() - x, q.y() - y) >= min_spacing;
    if (ok) out.emplace_back(x, y, 0.15 * x - 0.05 * y);
  }
  return out;
}

RigidTransform zRotation(double deg, const Eigen::Vector3d& t) {
  RigidTransform T;
  T.R = Eigen::AngleAxisd(deg * M_PI / 180.0, Eigen::Vector3d::UnitZ()).toRotationMatrix();
  T.t = t;
  return T;
}
}  // namespace

TEST(Cn, AngleIsCounterClockwiseEq6) {
  const Eigen::Vector2d a(1, 0);
  EXPECT_NEAR(cnAngle(a, {1, 0}), 0.0, 1e-12);
  EXPECT_NEAR(cnAngle(a, {0, 1}), M_PI / 2, 1e-12);
  EXPECT_NEAR(cnAngle(a, {-1, 0}), M_PI, 1e-12);
  EXPECT_NEAR(cnAngle(a, {0, -1}), 1.5 * M_PI, 1e-12);
}

TEST(Cn, FeatureMatrixSectorsAndNearestPerSector) {
  // p0 at origin, p1 on +x, p2 on +y, p3 on -x. N - 3 = 4 sectors of 90 deg.
  std::vector<Eigen::Vector3d> kp = {{0, 0, 0}, {1, 0, 0}, {0, 2, 0}, {-3, 0, 0}, {5, 0.1, 0}, {0.1, -4, 0}};
  CnParams p;
  p.N = 7;
  const auto m = cnFeatureMatrix(kp, 0, {{1, 2, 3}}, p);
  // encode1 starts at p1 (+x): sector 0 [0,90) holds p1 (closest, d=1) and kp4; sector 1 holds p2;
  // sector 2 holds p3; sector 3 holds kp5.
  EXPECT_EQ(m.index[0][0], 1);
  EXPECT_DOUBLE_EQ(m.dist[0][0], 1.0);
  EXPECT_EQ(m.index[0][1], 2);
  EXPECT_EQ(m.index[0][2], 3);
  EXPECT_EQ(m.index[0][3], 5);
  // encode2 starts at p2 (+y), counter-clockwise: sector 0 contains p2 itself (by definition);
  // p3 (-x) is exactly at 90 deg -> sector 1; kp5 (0.1, -4) is at ~181.4 deg -> sector 2;
  // p1 (+x, 270 deg) and kp4 (~271.1 deg) -> sector 3, closest is p1.
  EXPECT_EQ(m.index[1][0], 2);
  EXPECT_EQ(m.index[1][1], 3);
  EXPECT_EQ(m.index[1][2], 5);
  EXPECT_EQ(m.index[1][3], 1);
}

TEST(Cn, D12CountsWhenEncode2Sector0IsP1) {
  // p1 and p2 almost in the same direction: encode2 sector 0 (starting at p2) contains p1 (closer).
  std::vector<Eigen::Vector3d> kp = {{0, 0, 0}, {1, 0.01, 0}, {1.5, 0, 0}, {0, 3, 0}, {-4, 0, 0}};
  CnParams p;
  p.N = 7;
  CnStats st;
  cnFeatureMatrix(kp, 0, {{1, 2, 3}}, p, &st);
  EXPECT_EQ(st.enc2_sector0_is_p1, 1u);
}

TEST(Cn, DescriptorInvariantToHorizontalRigidMotion) {
  const auto kp = forest(60, 40, 2.0, 7);
  const RigidTransform T = zRotation(73.0, {12.3, -4.5, 1.7});
  std::vector<Eigen::Vector3d> kq;
  for (const auto& q : kp) kq.push_back(T.apply(q));
  CnParams p;
  const auto a = buildCnDescriptors(kp, p), b = buildCnDescriptors(kq, p);
  for (std::size_t i = 0; i < a.size(); ++i) {
    ASSERT_TRUE(a[i].valid);
    for (int k = 0; k < p.N; ++k) {
      ASSERT_EQ(a[i].idx[k], b[i].idx[k]) << i << " dim " << k;
      ASSERT_NEAR(a[i].val[k], b[i].val[k], 1e-9);
    }
  }
}

TEST(Cn, GatekeeperOneConsistentBitIsEnough) {
  CnDescriptor a, b;
  a.val = {1.0, 2.0, 3.0};
  a.idx = {0, 1, 2};
  b = a;
  b.val = {1.2, 2.2, 3.03};  // only bit 3 within 5 cm
  EXPECT_TRUE(cnGatekeeperPass(a, b, 0.05));
  b.val = {1.2, 2.2, 3.2};  // completely mismatched
  EXPECT_FALSE(cnGatekeeperPass(a, b, 0.05));
}

TEST(Cn, TriangleMatchAllowsPermutedVertices) {
  std::vector<Eigen::Vector3d> s = {{0, 0, 0}, {4, 0, 0}, {0, 3, 0}};
  // same triangle, vertices listed in a different order and moved
  std::vector<Eigen::Vector3d> t = {{10, 13, 0}, {10, 10, 0}, {14, 10, 0}};
  std::size_t ntri = 0;
  const auto pr = cnTriangleMatch({0, 1, 2}, {0, 1, 2}, s, t, 0.05, &ntri);
  EXPECT_EQ(ntri, 1u);
  ASSERT_EQ(pr.size(), 3u);
  for (const auto& [i, j] : pr) {
    if (i == 0) EXPECT_EQ(j, 1);
    if (i == 1) EXPECT_EQ(j, 2);
    if (i == 2) EXPECT_EQ(j, 0);
  }
  // one side off by 10 cm -> no match
  t[0].y() = 13.1;
  EXPECT_TRUE(cnTriangleMatch({0, 1, 2}, {0, 1, 2}, s, t, 0.05).empty());
}

TEST(Registration, KabschExactAndDegenerate) {
  const auto kp = forest(10, 20, 1.0, 3);
  RigidTransform T = zRotation(-121.0, {3, 4, -2});
  T.R = T.R * Eigen::AngleAxisd(0.02, Eigen::Vector3d::UnitX()).toRotationMatrix();
  std::vector<Eigen::Vector3d> d;
  for (const auto& q : kp) d.push_back(T.apply(q));
  const auto E = kabsch(kp, d);
  ASSERT_TRUE(E.has_value());
  EXPECT_LT(rotationErrorDeg(E->R, T.R), 1e-8);
  EXPECT_LT((E->t - T.t).norm(), 1e-8);
  EXPECT_FALSE(kabsch({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}, {{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}).has_value());
  EXPECT_FALSE(kabsch({{0, 0, 0}, {1, 0, 0}}, {{0, 0, 0}, {1, 0, 0}}).has_value());
}

TEST(Registration, RansacToleratesWrongPairs) {
  const auto kp = forest(30, 30, 1.0, 5);
  const RigidTransform T = zRotation(40.0, {1, 2, 0.5});
  std::vector<Eigen::Vector3d> d;
  for (const auto& q : kp) d.push_back(T.apply(q));
  // 18 of 30 pairs wrong: targets replaced by random points (pairwise swaps would only add a
  // symmetric term to the cross-covariance and can leave the SVD rotation exact).
  Rng g{17};
  for (int k = 0; k < 18; ++k) d[k] = Eigen::Vector3d((2 * g.uni() - 1) * 30, (2 * g.uni() - 1) * 30, g.uni());
  const auto plain = kabsch(kp, d);
  const auto r = kabschRansac(kp, d, RansacParams{});
  ASSERT_TRUE(r.T.has_value());
  EXPECT_LT(rotationErrorDeg(r.T->R, T.R), 1e-4);  // acos near 1 limits precision
  EXPECT_GE(r.inliers, 12u);
  ASSERT_TRUE(plain.has_value());
  EXPECT_GT(rotationErrorDeg(plain->R, T.R), 1.0);  // plain SVD is pulled off by wrong pairs
}

TEST(Registration, PointErrorIsEq8) {
  RigidTransform gt, est;
  est.t = {0.3, 0.4, 0};  // pure 0.5 m shift
  std::vector<Eigen::Vector3d> pts = {{1, 2, 3}, {-5, 0, 2}, {10, 10, -1}};
  EXPECT_NEAR(pointError(est, gt, pts), 0.5, 1e-12);
  est = zRotation(90, {0, 0, 0});
  // |R p - p| for p = (1,0,0) is sqrt(2)
  EXPECT_NEAR(pointError(est, gt, {{1, 0, 0}}), std::sqrt(2.0), 1e-12);
}

// End to end: source forest; target = rigid motion of 75% of the stems + 10 extra stems,
// 1 cm horizontal noise on every stem. CN matching at the paper's 5 cm must recover the motion.
TEST(CnEndToEnd, RecoversKnownTransformOnSyntheticForest) {
  const auto src = forest(80, 40, 2.0, 11);
  const RigidTransform T = zRotation(137.0, {-8.0, 5.5, 0.4});
  Rng g{99};
  std::vector<Eigen::Vector3d> tgt;
  for (std::size_t i = 0; i < src.size(); ++i) {
    if (i % 4 == 3) continue;  // 25% missing
    Eigen::Vector3d q = T.apply(src[i]);
    q.x() += 0.01 * g.normal();
    q.y() += 0.01 * g.normal();
    tgt.push_back(q);
  }
  for (const auto& q : forest(10, 40, 2.0, 12)) tgt.push_back(T.apply(q) + Eigen::Vector3d(0.7, 0.7, 0));
  CnParams p;
  const auto sd = buildCnDescriptors(src, p), td = buildCnDescriptors(tgt, p);
  const auto m = matchCn(sd, td, src, tgt, p);
  ASSERT_GE(m.pairs.size(), 3u) << "candidates " << m.candidates << " best score " << m.best_score;
  std::size_t correct = 0;
  std::vector<Eigen::Vector3d> a, b;
  for (const auto& [i, j] : m.pairs) {
    a.push_back(src[i]);
    b.push_back(tgt[j]);
    correct += (T.apply(src[i]) - tgt[j]).head<2>().norm() < 0.1;
  }
  EXPECT_GE(double(correct) / m.pairs.size(), 0.8) << correct << " / " << m.pairs.size();
  const auto r = kabschRansac(a, b, RansacParams{});
  ASSERT_TRUE(r.T.has_value());
  EXPECT_LT(rotationErrorDeg(r.T->R, T.R), 0.5);
  EXPECT_LT((r.T->t - T.t).norm(), 0.2);
}
