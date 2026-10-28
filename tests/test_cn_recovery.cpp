#include <gtest/gtest.h>
#include <Eigen/Geometry>
#include "cm/cn_recovery.hpp"
#include "cm/registration.hpp"

using namespace cm;
namespace {
std::vector<Eigen::Vector3d> stems() {
  return {{0,0,0},{2,0,.4},{.3,3,.1},{4,1,-.3},{-2,2,.8},{3,5,-.2},{-3,-1,.6},{1,-4,-.1}};
}
RigidTransform pose() {
  RigidTransform t;
  t.R=(Eigen::AngleAxisd(1.1,Eigen::Vector3d::UnitZ()) *
       Eigen::AngleAxisd(.05,Eigen::Vector3d::UnitX())).toRotationMatrix();
  t.t={-4,2,1};return t;
}
CnMatchResult sparseEvidence() {
  CnMatchResult m;
  // A high best score bypasses the paper's triangle fallback, despite just
  // two final matches. Descriptor dimensions still support the other stems.
  m.best_score=100;m.pairs={{0,0},{1,1}};
  CnCandidateScore c;
  c.source=0;c.target=0;c.score=100;
  for (int i=1;i<8;++i)c.evidence.emplace_back(i,i);
  c.evidence.emplace_back(2,6);c.evidence.emplace_back(4,2);
  m.ranked_candidates.push_back(c);return m;
}
std::vector<Eigen::Vector3d> transformed(const std::vector<Eigen::Vector3d>& s) {
  std::vector<Eigen::Vector3d> t;for(const auto& p:s)t.push_back(pose().apply(p));return t;
}
}  // namespace

TEST(CnRecovery, OffPreservesOriginalPairs) {
  const auto s=stems(),t=transformed(s);const auto m=sparseEvidence();
  const auto r=recoverCn(m,s,t,CnParams{},CnRecoveryParams{});
  EXPECT_EQ(r.pairs,m.pairs);EXPECT_FALSE(r.triggered);EXPECT_FALSE(r.accepted);
}
TEST(CnRecovery, DoesNotReplaceEnoughInitialMatches) {
  auto m=sparseEvidence();m.pairs={{0,0},{1,1},{2,2}};
  const auto s=stems(),t=transformed(s);
  CnRecoveryParams p;p.mode=CnRecoveryMode::EvidenceConsensus;
  const auto r=recoverCn(m,s,t,CnParams{},p);
  EXPECT_EQ(r.pairs,m.pairs);EXPECT_FALSE(r.triggered);
}
TEST(CnRecovery, HighScoreScarcityRecoversFullSixDofFromEvidence) {
  const auto s=stems(),t=transformed(s);const auto m=sparseEvidence();
  CnRecoveryParams p;p.mode=CnRecoveryMode::EvidenceConsensus;
  const auto r=recoverCn(m,s,t,CnParams{},p);
  ASSERT_TRUE(r.accepted);EXPECT_GE(r.support,8u);
  std::vector<Eigen::Vector3d> a,b;
  for(const auto& [i,j]:r.pairs){EXPECT_EQ(i,j);a.push_back(s[i]);b.push_back(t[j]);}
  const auto fitted=kabsch(a,b);ASSERT_TRUE(fitted.has_value());
  EXPECT_LT(rotationErrorDeg(fitted->R,pose().R),1e-4);
  EXPECT_LT((fitted->t-pose().t).norm(),1e-9);
  EXPECT_GT(std::abs(fitted->R(2,1)),.01); // real tilt; not yaw-only
}
TEST(CnRecovery, TriangleRecoveryAlsoTriggersOnHighScoreScarcity) {
  const auto s=stems(),t=transformed(s);
  CnRecoveryParams p;p.mode=CnRecoveryMode::Triangles;
  const auto r=recoverCn(sparseEvidence(),s,t,CnParams{},p);
  ASSERT_TRUE(r.accepted);EXPECT_GE(r.pairs.size(),6u);
  for(const auto& [i,j]:r.pairs)EXPECT_EQ(i,j);
}
TEST(CnRecovery, ConsistentHorizontalGeometryDoesNotIgnoreWrongHeight) {
  const auto s=stems();auto t=transformed(s);
  for(std::size_t i=0;i<t.size();++i)t[i].z()+=10*i*i;
  CnRecoveryParams p;p.mode=CnRecoveryMode::EvidenceConsensus;
  const auto m=sparseEvidence();const auto r=recoverCn(m,s,t,CnParams{},p);
  EXPECT_FALSE(r.accepted);EXPECT_EQ(r.pairs,m.pairs);
}
TEST(CnRecovery, TooLittleSupportKeepsOriginalPairs) {
  const auto s=stems(),t=transformed(s);auto m=sparseEvidence();
  m.ranked_candidates[0].evidence={{1,1},{2,2},{3,3}};
  CnRecoveryParams p;p.mode=CnRecoveryMode::EvidenceConsensus;
  const auto r=recoverCn(m,s,t,CnParams{},p);
  EXPECT_EQ(r.support,4u);EXPECT_FALSE(r.accepted);EXPECT_EQ(r.pairs,m.pairs);
}
