#pragma once
#include <vector>

#include "cm/types.hpp"

namespace cm {

// Paper: 1 cm voxels, keep the ORIGINAL point closest to the voxel centroid.
// (pcl::VoxelGrid outputs the centroid itself; pcl::UniformSampling keeps the
// point nearest the voxel centre - neither matches, hence this function.)
// Returns indices into `cloud`, ordered by voxel key; equal distances are
// broken by coordinates, so the selected points do not depend on input order.
std::vector<int> voxelNearestToCentroid(const Cloud& cloud, const std::vector<int>& indices, double leaf);

}  // namespace cm
