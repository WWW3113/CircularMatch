#pragma once
#include <vector>

#include "cm/config.hpp"
#include "cm/types.hpp"

namespace cm {

// Euclidean clustering of cloud[indices]. Each returned cluster holds indices
// into `cloud`, sorted ascending; clusters are ordered by their smallest index
// so cluster ids are stable for identical inputs.
std::vector<std::vector<int>> euclideanClusters(const Cloud& cloud, const std::vector<int>& indices,
                                                const ClusterParams& p);

}  // namespace cm
