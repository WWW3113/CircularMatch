#include <gtest/gtest.h>
#include <Eigen/Geometry>
#include "cm/geometry_refinement.hpp"
using namespace cm;
namespace {
std::pair<std::vector<SurfaceLevel>,std::vector<SurfaceLevel>> scene(const RigidTransform& truth,bool outliers) {
  std::vector<SurfaceLevel> source,target;
  for(double voxel:{.5,.2,.1}) {
    SurfaceLevel a,b;a.voxel=b.voxel=voxel;
    for(int plane=0;plane<3;++plane)for(int i=-12;i<=12;++i)for(int j=-12;j<=12;++j) {
      Eigen::Vector3d q=Eigen::Vector3d::Zero(),n=Eigen::Vector3d::Zero();n[plane]=1;
      q[(plane+1)%3]=.13*i;q[(plane+2)%3]=.13*j;
      b.points.push_back({q,n});a.points.push_back({truth.R.transpose()*(q-truth.t),truth.R.transpose()*n});
    }
    if(outliers)for(int i=0;i<500;++i)a.points.push_back({Eigen::Vector3d(8+i*.01,8,6),Eigen::Vector3d::UnitX()});
    source.push_back(a);target.push_back(b);
  }return {source,target};
}
RigidTransform truthPose() {
  RigidTransform t;t.R=(Eigen::AngleAxisd(.08,Eigen::Vector3d::UnitX())*Eigen::AngleAxisd(-.09,Eigen::Vector3d::UnitY())*Eigen::AngleAxisd(.13,Eigen::Vector3d::UnitZ())).toRotationMatrix();t.t=Eigen::Vector3d(.2,-.15,.1);return t;
}
}
TEST(GeometryRefinement, RecoversTrueSixDof) {
  const auto truth=truthPose();const auto [a,b]=scene(truth,false);RigidTransform initial=truth;
  initial.R=Eigen::AngleAxisd(.04,Eigen::Vector3d(1,1,0).normalized()).toRotationMatrix()*initial.R;initial.t+=Eigen::Vector3d(.08,-.06,.07);
  const auto result=refineGeometry(a,b,initial);
  EXPECT_TRUE(result.solved);EXPECT_LT(rotationErrorDeg(result.pose.R,truth.R),.1);EXPECT_LT((result.pose.t-truth.t).norm(),.005);
}
TEST(GeometryRefinement, RejectsDistantOutliers) {
  const auto truth=truthPose();const auto [a,b]=scene(truth,true);RigidTransform initial=truth;initial.t+=Eigen::Vector3d(.08,-.06,.07);
  const auto result=refineGeometry(a,b,initial);
  EXPECT_LT(rotationErrorDeg(result.pose.R,truth.R),.1);EXPECT_LT((result.pose.t-truth.t).norm(),.005);
}
TEST(GeometryRefinement, PlanarDegeneracyPreservesPose) {
  std::vector<SurfaceLevel> a,b;
  for(double h:{.5,.2,.1}) {
    SurfaceLevel level;level.voxel=h;for(int i=-12;i<=12;++i)for(int j=-12;j<=12;++j)level.points.push_back({Eigen::Vector3d(i*.1,j*.1,0),Eigen::Vector3d::UnitZ()});
    a.push_back(level);b.push_back(level);
  }
  RigidTransform initial;initial.t=Eigen::Vector3d(.1,.1,.03);const auto result=refineGeometry(a,b,initial);
  EXPECT_FALSE(result.solved);EXPECT_LT((result.pose.t-initial.t).norm(),1e-12);
}
TEST(GeometryRefinement, EmptySurfacesDoNotInventTransform) {
  std::vector<SurfaceLevel> a(3);RigidTransform initial;initial.t=Eigen::Vector3d(1,2,3);const auto result=refineGeometry(a,a,initial);
  EXPECT_FALSE(result.solved);EXPECT_LT((result.pose.t-initial.t).norm(),1e-12);
}
