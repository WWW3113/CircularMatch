#include "cm/cluster.hpp"

#include <pcl/search/kdtree.h>
#include <pcl/segmentation/extract_clusters.h>

#include <algorithm>

namespace cm {

std::vector<std::vector<int>> euclideanClusters(const Cloud& cloud, const std::vector<int>& indices,
                                                const ClusterParams& p) {
  std::vector<std::vector<int>> out;
  if (indices.empty()) return out;
  Cloud::Ptr sub(new Cloud);
  sub->reserve(indices.size());
  for (int i : indices) sub->push_back(cloud[i]);
  pcl::search::KdTree<PointT>::Ptr tree(new pcl::search::KdTree<PointT>);
  tree->setInputCloud(sub);
  pcl::EuclideanClusterExtraction<PointT> ec;
  ec.setClusterTolerance(p.tolerance);
  ec.setMinClusterSize(p.min_points);
  ec.setMaxClusterSize(p.max_points);
  ec.setSearchMethod(tree);
  ec.setInputCloud(sub);
  std::vector<pcl::PointIndices> found;
  ec.extract(found);
  for (const auto& c : found) {
    std::vector<int> v;
    v.reserve(c.indices.size());
    for (auto k : c.indices) v.push_back(indices[k]);
    std::sort(v.begin(), v.end());
    out.push_back(std::move(v));
  }
  std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) { return a.front() < b.front(); });
  return out;
}

}  // namespace cm
