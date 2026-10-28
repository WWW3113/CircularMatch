#include "cm/geometry_refinement.hpp"
#include <Eigen/Dense>
#include <Eigen/Geometry>
#include <pcl/search/kdtree.h>
#include <algorithm>
#include <cmath>
#include <map>
#include "cm/types.hpp"
namespace cm {
namespace {
struct Index {
  Cloud::Ptr cloud{new Cloud};pcl::search::KdTree<PointT> tree;
  explicit Index(const SurfaceLevel& level) {
    cloud->reserve(level.points.size());for(const auto& q:level.points)cloud->push_back(PointT(q.p.x(),q.p.y(),q.p.z()));
    tree.setInputCloud(cloud);
  }
};
std::vector<std::size_t> sample(std::size_t n,std::size_t cap) {
  std::vector<std::size_t> ids;const auto k=std::min(n,cap);ids.reserve(k);
  for(std::size_t i=0;i<k;++i)ids.push_back(i*n/k);return ids;
}
GeometryScore directional(const SurfaceLevel& source,const SurfaceLevel& target,const RigidTransform& pose,const Index& index,double gate,std::size_t cap) {
  const auto ids=sample(source.points.size(),cap);std::size_t support=0;double loss=0;
#pragma omp parallel for reduction(+:support,loss) schedule(static)
  for(std::size_t k=0;k<ids.size();++k) {
    const auto& s=source.points[ids[k]];const auto p=pose.apply(s.p);pcl::Indices ix(1);std::vector<float> ds(1);
    if(index.tree.nearestKSearch(PointT(p.x(),p.y(),p.z()),1,ix,ds)==1 && ds[0]<gate*gate &&
       std::abs((pose.R*s.n).dot(target.points[ix[0]].n))>.7) {++support;loss+=ds[0];}
  }
  return {ids.empty()?0:double(support)/ids.size(),support?std::sqrt(loss/support):1e9,support};
}
}
GeometryScore geometryScore(const SurfaceLevel& source,const SurfaceLevel& target,const RigidTransform& pose) {
  if(source.points.empty()||target.points.empty())return {};
  Index si(source),ti(target);RigidTransform inv;inv.R=pose.R.transpose();inv.t=-inv.R*pose.t;
  const double gate=3*source.voxel;
  const auto a=directional(source,target,pose,ti,gate,8000),b=directional(target,source,inv,si,gate,8000);
  return {std::sqrt(a.overlap*b.overlap),(a.rmse+b.rmse)/2,std::min(a.support,b.support)};
}
RefineResult refineGeometry(const std::vector<SurfaceLevel>& source,const std::vector<SurfaceLevel>& target,const RigidTransform& initial) {
  RefineResult result;result.pose=initial;if(source.size()!=3||target.size()!=3)return result;
  result.before=geometryScore(source.back(),target.back(),initial);
  const int budget[3]={30,25,20};
  for(std::size_t level=0;level<3;++level) {
    const auto& src=source[level];const auto& tgt=target[level];if(src.points.size()<30||tgt.points.size()<30)continue;
    const auto ids=sample(src.points.size(),20000);Index index(tgt);const double gate=3*src.voxel;
    struct Match {Eigen::Vector3d p,q,n;double d=0,r=0;int target=-1;};
    for(int iteration=0;iteration<budget[level];++iteration) {
      std::vector<Match> candidates(ids.size());std::vector<char> valid(ids.size(),0);
#pragma omp parallel for schedule(static)
      for(std::size_t k=0;k<ids.size();++k) {
        const auto& original=src.points[ids[k]];const auto p=result.pose.apply(original.p);pcl::Indices ix(1);std::vector<float> ds(1);
        if(index.tree.nearestKSearch(PointT(p.x(),p.y(),p.z()),1,ix,ds)!=1||ds[0]>=gate*gate)continue;
        const auto& t=tgt.points[ix[0]];if(std::abs((result.pose.R*original.n).dot(t.n))<.7)continue;
        candidates[k]={p,t.p,t.n,ds[0],t.n.dot(p-t.p),ix[0]};valid[k]=1;
      }
      std::vector<Match> matches;for(std::size_t k=0;k<candidates.size();++k)if(valid[k])matches.push_back(candidates[k]);
      std::sort(matches.begin(),matches.end(),[](const Match&a,const Match&b){return a.d<b.d;});
      std::vector<Match> unique;std::vector<char> used(tgt.points.size(),0);
      for(const auto& m:matches)if(!used[m.target]){used[m.target]=1;unique.push_back(m);}
      if(unique.size()<30)break;unique.resize(std::max<std::size_t>(30,std::size_t(unique.size()*.7)));
      std::vector<double> residual;for(const auto& m:unique)residual.push_back(std::abs(m.r));
      const auto middle=residual.begin()+residual.size()/2;std::nth_element(residual.begin(),middle,residual.end());
      const double huber=std::max(.05*src.voxel,2*(*middle));
      Eigen::Matrix<double,6,6> H=Eigen::Matrix<double,6,6>::Zero();Eigen::Matrix<double,6,1> b=Eigen::Matrix<double,6,1>::Zero();
      for(const auto& m:unique) {
        Eigen::Matrix<double,6,1> j;j.head<3>()=m.p.cross(m.n);j.tail<3>()=m.n;
        const double w=std::abs(m.r)<=huber?1:huber/std::abs(m.r);H.noalias()+=w*j*j.transpose();b.noalias()-=w*j*m.r;
      }
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double,6,6>> eig(H);
      if(eig.eigenvalues()[0]<1e-8*eig.eigenvalues()[5])break;
      auto dx=H.ldlt().solve(b).eval();if(!dx.allFinite())break;
      const double angle=dx.head<3>().norm(),distance=dx.tail<3>().norm();
      const double factor=std::min({1.,angle>0?.15/angle:1.,distance>0?src.voxel/distance:1.});dx*=factor;
      const double rotation=dx.head<3>().norm();Eigen::Matrix3d R=Eigen::Matrix3d::Identity();
      if(rotation>1e-12)R=Eigen::AngleAxisd(rotation,dx.head<3>()/rotation).toRotationMatrix();
      result.pose.t=R*result.pose.t+dx.tail<3>();result.pose.R=R*result.pose.R;result.solved=true;++result.iterations;
      if(dx.head<3>().norm()<1e-6&&dx.tail<3>().norm()<1e-5)break;
    }
  }
  result.after=geometryScore(source.back(),target.back(),result.pose);
  // Fixed geometry-only acceptance; preserve a valid coarse pose on regression.
  const double before=result.before.overlap/(.03+result.before.rmse),after=result.after.overlap/(.03+result.after.rmse);
  if(after+1e-9<before){result.pose=initial;result.after=result.before;result.solved=false;}
  return result;
}
}
