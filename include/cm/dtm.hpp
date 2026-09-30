// Grid DTM ("lowest point" family, 論文未指定 details):
//  1. per cell, one ground sample (its full x,y,z is kept). The paper does not
//     specify the DTM method; this is OUR implementation choice:
//       supported_lowest (default): the lowest point that has >= support_count
//         points within support_dz above it (drops isolated below-ground noise
//         independently of how many stem/canopy points share the cell);
//       percentile (kept for comparison): the k-th z percentile. Biased upward
//         when stem/canopy points outnumber ground points in a cell (see test
//         Dtm.DenseStemCellBiasesPercentile).
//  2. ground samples higher than a plane fitted to their neighbours by more
//     than outlier_above are rejected (occluded cells that only hold stems);
//  3. every cell gets a local plane z = a + b(x-cx) + c(y-cy), least squares
//     over ground samples in a (2r+1)^2 window; r grows from fit_radius up to
//     fill_max_radius for empty cells. The plane both smooths and fills holes,
//     and, unlike assigning the percentile height to the cell centre, it is
//     unbiased on slopes (the percentile point lies at the downhill edge).
//  4. height(x,y) blends the planes of the 4 nearest cell centres, each
//     evaluated at (x,y), with bilinear weights (exact on planar terrain).
#pragma once
#include <Eigen/Core>
#include <optional>
#include <vector>

#include "cm/config.hpp"
#include "cm/types.hpp"

namespace cm {

struct DtmStats {
  int nx = 0, ny = 0;
  std::size_t cells_with_sample = 0;
  std::size_t samples_rejected = 0;
  std::size_t cells_valid = 0;   // cells with a plane
  std::size_t cells_filled = 0;  // valid cells without own sample
};

class Dtm {
 public:
  static Dtm build(const Cloud& cloud, const DtmParams& p);
  std::optional<double> height(double x, double y) const;
  const DtmStats& stats() const { return stats_; }
  double cellSize() const { return cell_; }

 private:
  struct Plane {
    double a = 0, b = 0, c = 0;
    bool valid = false;
  };
  double eval(int i, int j, double x, double y) const;
  double x0_ = 0, y0_ = 0, cell_ = 1;
  int nx_ = 0, ny_ = 0;
  std::vector<Plane> planes_;
  DtmStats stats_;
};

// Indices of points with zmin - neg_tol <= z - DTM(x,y) <= zmax.
// Points outside DTM coverage are dropped and counted in *no_dtm.
std::vector<int> heightFilter(const Cloud& cloud, const Dtm& dtm, const HeightParams& p, std::size_t* no_dtm = nullptr);

}  // namespace cm
