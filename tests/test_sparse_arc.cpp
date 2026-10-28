#include <gtest/gtest.h>
#include "cm/sparse_arc.hpp"
using namespace cm;
namespace {double rad(double d){return d*std::acos(-1.0)/180;}}
TEST(SparseArc, SparseHalfCircleKeepsItsExtent) {
  std::vector<double> a;for(int d=0;d<=180;d+=10)a.push_back(rad(d));
  EXPECT_GE(trimmedArcDegrees(a),150);
  // Only 19 occupied five-degree bins would score 95 degrees.
  EXPECT_GT(trimmedArcDegrees(a),100);
}
TEST(SparseArc, WrapAroundAndRotationDoNotChangeExtent) {
  std::vector<double> a,b;for(int d=-20;d<=20;d+=2){a.push_back(rad(d));b.push_back(rad(d+137));}
  EXPECT_NEAR(trimmedArcDegrees(a),trimmedArcDegrees(b),1e-10);
  EXPECT_LT(trimmedArcDegrees(a),40);
}
TEST(SparseArc, IsolatedOutliersCannotMakeNarrowStripPass) {
  std::vector<double> a;for(int d=0;d<100;++d)a.push_back(rad(d*.3));
  a.push_back(rad(120));a.push_back(rad(240));a.push_back(rad(300));
  EXPECT_LT(trimmedArcDegrees(a),35);
}
TEST(SparseArc, InvalidAndTinyInputs) {
  EXPECT_DOUBLE_EQ(trimmedArcDegrees({}),0);
  EXPECT_DOUBLE_EQ(trimmedArcDegrees({0}),0);
  EXPECT_THROW(trimmedArcDegrees({0,1},0),std::invalid_argument);
}
