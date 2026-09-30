#pragma once
#include <vector>

#include "cm/config.hpp"
#include "cm/types.hpp"

namespace cm {

struct NormalResult {
  NormalCloud::Ptr normals;          // e3 = eigenvector of the smallest eigenvalue (NaN if invalid)
  std::vector<float> verticality;    // 1 - |z . e3|  (NaN if invalid)
  std::vector<char> valid;           // false: fewer than min_neighbors within radius
  std::size_t n_invalid = 0;
};

// Octree radius search (paper: 10 cm), covariance (paper eq. 1-2), eigen
// decomposition (eq. 3), verticality (eq. 4). Neighbourhoods are taken in
// `cloud` itself. Per-point results do not depend on thread scheduling.
NormalResult computeNormals(const Cloud& cloud, const NormalParams& p);

}  // namespace cm
