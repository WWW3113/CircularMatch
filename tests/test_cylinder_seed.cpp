// Verifies on the INSTALLED PCL that the RANSAC sampling is controlled by our seed.
#include <gtest/gtest.h>

#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_cylinder.h>

#include <cmath>

#include "cm/cylinder.hpp"
#include "cm/normals.hpp"
#include "cm/retention.hpp"

using namespace cm;

namespace {
struct Cyl {
  Cloud c;
  NormalCloud n;
};
Cyl noisyCylinder(double r, int npts, double outlier_frac) {
  Cyl out;
  std::uint64_t s = 77;
  auto uni = [&] { s = splitmix64(s); return (s >> 11) * 0x1.0p-53; };
  for (int i = 0; i < npts; ++i) {
    PointT p;
    NormalT n;
    if (uni() < outlier_frac) {
      p = PointT(float(uni() - 0.5), float(uni() - 0.5), float(2 * uni()));
      n.normal_x = float(uni() - 0.5); n.normal_y = float(uni() - 0.5); n.normal_z = float(uni() - 0.5);
    } else {
      const double th = 2 * M_PI * uni(), z = 2 * uni();
      const double e = 0.004 * (uni() - 0.5);
      p = PointT(float((r + e) * std::cos(th)), float((r + e) * std::sin(th)), float(z));
      n.normal_x = float(std::cos(th)); n.normal_y = float(std::sin(th)); n.normal_z = 0;
    }
    out.c.push_back(p);
    out.n.push_back(n);
  }
  return out;
}
}  // namespace

TEST(CylinderSeed, SameSeedSameSamplesDifferentSeedDifferentSamples) {
  const Cyl cy = noisyCylinder(0.2, 500, 0.3);
  const auto a = drawSamplesForTest(cy.c, cy.n, 1234, 20);
  const auto b = drawSamplesForTest(cy.c, cy.n, 1234, 20);
  const auto c = drawSamplesForTest(cy.c, cy.n, 999, 20);
  EXPECT_EQ(a, b);
  EXPECT_NE(a, c);
}

TEST(CylinderSeed, DefaultPclModelIsFixedAt12345) {
  // Documented behaviour of the installed PCL: random=false seeds with 12345,
  // so an un-reseeded model draws the same samples as our reseed(12345).
  const Cyl cy = noisyCylinder(0.2, 500, 0.3);
  Cloud::Ptr c(new Cloud(cy.c));
  NormalCloud::Ptr n(new NormalCloud(cy.n));
  struct Probe : pcl::SampleConsensusModelCylinder<PointT, NormalT> {
    explicit Probe(const Cloud::ConstPtr& cc) : pcl::SampleConsensusModelCylinder<PointT, NormalT>(cc, false) {}
    using pcl::SampleConsensusModelCylinder<PointT, NormalT>::getSamples;
  } m(c);
  m.setInputNormals(n);
  std::vector<std::vector<int>> def;
  for (int k = 0; k < 20; ++k) {
    int it = 0;
    pcl::Indices s;
    m.getSamples(it, s);
    def.emplace_back(s.begin(), s.end());
  }
  EXPECT_EQ(def, drawSamplesForTest(cy.c, cy.n, 12345, 20));
}

TEST(CylinderSeed, FitIsReproducibleAndAccurate) {
  const Cyl cy = noisyCylinder(0.2, 3000, 0.2);
  std::vector<int> idx(cy.c.size());
  for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = int(i);
  CylinderParams p;
  p.min_inlier_ratio = 0.5;
  const auto f1 = fitCylinder(cy.c, cy.n, idx, p, 42);
  const auto f2 = fitCylinder(cy.c, cy.n, idx, p, 42);
  ASSERT_EQ(f1.fail, FitFail::None) << toString(f1.fail);
  EXPECT_EQ(f1.radius, f2.radius);
  EXPECT_EQ(f1.axis_point, f2.axis_point);
  EXPECT_EQ(f1.n_inliers, f2.n_inliers);
  EXPECT_NEAR(f1.radius, 0.2, 0.005);
  EXPECT_LT(f1.tilt_deg, 1.0);
}

TEST(CylinderSeed, FailureReasons) {
  CylinderParams p;
  const Cyl small = noisyCylinder(0.2, 10, 0);
  std::vector<int> idx10(10);
  for (int i = 0; i < 10; ++i) idx10[i] = i;
  EXPECT_EQ(fitCylinder(small.c, small.n, idx10, p, 1).fail, FitFail::TooFewPoints);

  const Cyl big = noisyCylinder(2.0, 3000, 0);  // radius 2 m > radius_max
  std::vector<int> idx(3000);
  for (int i = 0; i < 3000; ++i) idx[i] = i;
  const auto f = fitCylinder(big.c, big.n, idx, p, 1);
  EXPECT_EQ(f.fail, FitFail::RadiusOutOfRange) << toString(f.fail) << " r=" << f.radius;
}

TEST(CylinderChecks, ExtraChecksCanBeDisabled) {
  // A narrow strip (40 degrees) of an r = 0.3 m cylinder: arc coverage is low.
  Cyl strip;
  std::uint64_t s = 5;
  auto uni = [&] { s = splitmix64(s); return (s >> 11) * 0x1.0p-53; };
  for (int i = 0; i < 2000; ++i) {
    const double th = (uni() - 0.5) * 40.0 * M_PI / 180.0, z = 2 * uni();
    strip.c.push_back(PointT(float(0.3 * std::cos(th)), float(0.3 * std::sin(th)), float(z)));
    NormalT n;
    n.normal_x = float(std::cos(th)); n.normal_y = float(std::sin(th)); n.normal_z = 0;
    strip.n.push_back(n);
  }
  std::vector<int> idx(strip.c.size());
  for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = int(i);
  CylinderParams on;
  CylinderParams off = on;
  off.check_normal_consistency = off.check_arc_coverage = false;
  const auto f_on = fitCylinder(strip.c, strip.n, idx, on, 3);
  const auto f_off = fitCylinder(strip.c, strip.n, idx, off, 3);
  EXPECT_TRUE(f_on.fail_mask & (1u << int(FitFail::ArcCoverageLow))) << f_on.arc_deg;
  EXPECT_FALSE(f_off.fail_mask & (1u << int(FitFail::ArcCoverageLow)));
  EXPECT_FALSE(f_off.fail_mask & (1u << int(FitFail::NormalInconsistent)));
  EXPECT_EQ(f_on.arc_deg, f_off.arc_deg);  // metric still computed and reported when disabled
  EXPECT_EQ(f_on.radius, f_off.radius);    // the checks never change the fit itself
}

// The post-fit filters that are not in the paper (radius, tilt, inlier ratio)
// can each be switched off; the fit itself is unchanged.
TEST(CylinderChecks, NotInPaperFiltersCanBeDisabled) {
  const Cyl cy = noisyCylinder(1.5, 800, 0.0);  // radius above radius_max = 1 m
  std::vector<int> idx(cy.c.size());
  for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = int(i);
  CylinderParams on;
  on.check_normal_consistency = on.check_arc_coverage = false;
  const CylinderFit a = fitCylinder(cy.c, cy.n, idx, on, 1);
  EXPECT_TRUE(a.fail_mask & (1u << int(FitFail::RadiusOutOfRange)));
  CylinderParams off = on;
  off.check_radius = false;
  const CylinderFit b = fitCylinder(cy.c, cy.n, idx, off, 1);
  EXPECT_EQ(b.fail, FitFail::None);
  EXPECT_NEAR(b.radius, 1.5, 0.01);
  EXPECT_DOUBLE_EQ(a.radius, b.radius);
}
