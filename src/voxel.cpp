#include "cm/voxel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace cm {

std::vector<int> voxelNearestToCentroid(const Cloud& cloud, const std::vector<int>& indices, double leaf) {
  if (!(leaf > 0)) throw std::runtime_error("voxel: leaf must be > 0");
  struct Item {
    std::array<std::int64_t, 3> key;
    int idx;
  };
  std::vector<Item> items;
  items.reserve(indices.size());
  for (int i : indices) {
    const auto& p = cloud[i];
    items.push_back({{static_cast<std::int64_t>(std::floor(p.x / leaf)), static_cast<std::int64_t>(std::floor(p.y / leaf)),
                      static_cast<std::int64_t>(std::floor(p.z / leaf))},
                     i});
  }
  // Within a voxel, order by coordinates (then index for exact duplicates) so
  // the centroid sum and the tie-break do not depend on input order.
  std::sort(items.begin(), items.end(), [&](const Item& a, const Item& b) {
    if (a.key != b.key) return a.key < b.key;
    const auto &p = cloud[a.idx], &q = cloud[b.idx];
    if (p.x != q.x) return p.x < q.x;
    if (p.y != q.y) return p.y < q.y;
    if (p.z != q.z) return p.z < q.z;
    return a.idx < b.idx;
  });
  std::vector<int> out;
  for (std::size_t b = 0; b < items.size();) {
    std::size_t e = b;
    double cx = 0, cy = 0, cz = 0;
    while (e < items.size() && items[e].key == items[b].key) {
      const auto& p = cloud[items[e].idx];
      cx += p.x; cy += p.y; cz += p.z;
      ++e;
    }
    const double n = static_cast<double>(e - b);
    cx /= n; cy /= n; cz /= n;
    int best = items[b].idx;
    double bestd = std::numeric_limits<double>::infinity();
    for (std::size_t k = b; k < e; ++k) {  // sorted by coordinates: strict < keeps the lexicographically smallest on ties
      const auto& p = cloud[items[k].idx];
      const double d = (p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy) + (p.z - cz) * (p.z - cz);
      if (d < bestd) { bestd = d; best = items[k].idx; }
    }
    out.push_back(best);
    b = e;
  }
  return out;
}

}  // namespace cm
