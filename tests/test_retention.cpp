#include <gtest/gtest.h>

#include "cm/retention.hpp"

using namespace cm;

namespace {
RetentionModel M(RetentionKind k) { return RetentionModel::make(k, RetentionParams{}); }
const RetentionKind kAll[] = {RetentionKind::Step, RetentionKind::LinearA, RetentionKind::LinearMid,
                              RetentionKind::Physical};
}  // namespace

TEST(Retention, StepValues) {
  const auto m = M(RetentionKind::Step);
  EXPECT_DOUBLE_EQ(m(0), 0.5);
  EXPECT_DOUBLE_EQ(m(4.999), 0.5);
  EXPECT_DOUBLE_EQ(m(5), 0.75);   // our completion: 5 belongs to the middle segment
  EXPECT_DOUBLE_EQ(m(9.999), 0.75);
  EXPECT_DOUBLE_EQ(m(10), 1.0);   // our completion: 10 belongs to the last segment
  EXPECT_DOUBLE_EQ(m(20), 1.0);
}

TEST(Retention, LinearAValues) {
  const auto m = M(RetentionKind::LinearA);
  EXPECT_DOUBLE_EQ(m(0), 0.5);
  EXPECT_DOUBLE_EQ(m(5), 0.75);
  EXPECT_DOUBLE_EQ(m(10), 1.0);
  EXPECT_DOUBLE_EQ(m(20), 1.0);
}

TEST(Retention, LinearMidValues) {
  const auto m = M(RetentionKind::LinearMid);
  EXPECT_DOUBLE_EQ(m(0), 0.375);
  EXPECT_DOUBLE_EQ(m(5), 0.625);
  EXPECT_DOUBLE_EQ(m(10), 0.875);
  EXPECT_DOUBLE_EQ(m(20), 1.0);
}

TEST(Retention, PhysicalValues) {
  const auto m = M(RetentionKind::Physical);
  EXPECT_DOUBLE_EQ(m(0), 0.1);  // = P_min
  EXPECT_DOUBLE_EQ(m(5), 0.25);
  EXPECT_DOUBLE_EQ(m(10), 1.0);
  EXPECT_DOUBLE_EQ(m(20), 1.0);
  EXPECT_DOUBLE_EQ(m(2), 0.1);  // (0.2)^2 = 0.04 < P_min
}

TEST(Retention, RangeAndMonotone) {
  for (auto k : kAll) {
    const auto m = M(k);
    double prev = -1;
    for (double d = 0; d <= 100; d += 0.01) {
      const double p = m(d);
      ASSERT_GE(p, 0.0) << toString(k) << " d=" << d;
      ASSERT_LE(p, 1.0) << toString(k) << " d=" << d;
      ASSERT_GE(p, prev) << toString(k) << " not monotone at d=" << d;
      prev = p;
    }
  }
}

TEST(Retention, ClampedForExtremeParameters) {
  RetentionModel m = M(RetentionKind::LinearA);
  m.intercept = -0.4;  // budget matching may go negative
  EXPECT_DOUBLE_EQ(m(0), 0.0);
  m.intercept = 1.3;
  EXPECT_DOUBLE_EQ(m(0), 1.0);
}

TEST(Retention, LinearAAtLeastStepEverywhere) {
  const auto a = M(RetentionKind::LinearA), s = M(RetentionKind::Step), mid = M(RetentionKind::LinearMid);
  for (double d = 0; d <= 50; d += 0.001) {
    ASSERT_GE(a(d), s(d)) << d;
    ASSERT_GE(a(d), mid(d)) << d;
  }
}

TEST(Retention, LinearMidSegmentMeansMatchStepUnderUniformD) {
  // Mean of 0.375 + 0.05 d over [0,5) = 0.5 and over [5,10) = 0.75 (uniform d only).
  const auto m = M(RetentionKind::LinearMid);
  double s1 = 0, s2 = 0;
  const int n = 100000;
  for (int i = 0; i < n; ++i) {
    s1 += m((i + 0.5) * 5.0 / n);
    s2 += m(5.0 + (i + 0.5) * 5.0 / n);
  }
  EXPECT_NEAR(s1 / n, 0.5, 1e-9);
  EXPECT_NEAR(s2 / n, 0.75, 1e-9);
}
