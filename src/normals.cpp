#include "cm/normals.hpp"

#include <pcl/octree/octree_search.h>

#include <Eigen/Eigenvalues>
#include <cmath>
#include <limits>

namespace cm {

NormalResult computeNormals(const Cloud& cloud, const NormalParams& p) {
  NormalResult r;
  const std::size_t n = cloud.size();
  r.normals.reset(new NormalCloud);
  r.normals->resize(n);
  r.verticality.assign(n, std::numeric_limits<float>::quiet_NaN());
  r.valid.assign(n, 0);
  if (n == 0) return r;

  Cloud::Ptr c(new Cloud(cloud));
  pcl::octree::OctreePointCloudSearch<PointT> octree(p.octree_res);
  octree.setInputCloud(c);
  octree.addPointsFromInputCloud();

  const float nan = std::numeric_limits<float>::quiet_NaN();
#pragma omp parallel for schedule(dynamic, 256)
  for (long long i = 0; i < static_cast<long long>(n); ++i) {
    pcl::Indices nb;
    std::vector<float> d2;
    octree.radiusSearch((*c)[i], p.radius, nb, d2);
    NormalT& out = (*r.normals)[i];
    if (static_cast<int>(nb.size()) < p.min_neighbors) {
      out.normal_x = out.normal_y = out.normal_z = out.curvature = nan;
      continue;
    }
    Eigen::Vector3d C = Eigen::Vector3d::Zero();
    for (auto k : nb) C += (*c)[k].getVector3fMap().cast<double>();
    C /= double(nb.size());
    Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();
    for (auto k : nb) {
      const Eigen::Vector3d dvec = (*c)[k].getVector3fMap().cast<double>() - C;
      cov += dvec * dvec.transpose();
    }
    cov /= double(nb.size());
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(cov);
    const Eigen::Vector3d e3 = es.eigenvectors().col(0);  // ascending eigenvalues
    const Eigen::Vector3d ev = es.eigenvalues();
    out.normal_x = float(e3.x());
    out.normal_y = float(e3.y());
    out.normal_z = float(e3.z());
    const double tr = ev.sum();
    out.curvature = tr > 0 ? float(ev(0) / tr) : 0.f;
    r.verticality[i] = float(1.0 - std::abs(e3.z()));
    r.valid[i] = 1;
  }
  for (char v : r.valid) r.n_invalid += v ? 0 : 1;
  return r;
}

}  // namespace cm
