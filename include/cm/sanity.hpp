// Guards against silent coordinate-system / unit errors, and converts the
// double-precision cloud to a float cloud in the scanner-centred local frame:
//   local = original - (x0, y0, z0)
// so that the scanner sits at the local origin and d is computed directly.
#pragma once
#include <Eigen/Core>
#include <string>
#include <vector>

#include "cm/config.hpp"
#include "cm/io.hpp"
#include "cm/types.hpp"

namespace cm {

struct SanityReport {
  Eigen::Vector3d bbox_min, bbox_max;      // original coordinates
  double nearest_horizontal = 0.0;         // nearest point to scanner, horizontal [m]
  double max_abs_local = 0.0;              // max |local coordinate| [m]
  std::size_t n_points = 0;
  std::vector<std::string> warnings;
};

struct LoadedCloud {
  Cloud::Ptr cloud;          // local frame, float
  Eigen::Vector3d offset;    // original = local + offset (= scanner position)
  SanityReport report;
};

// Throws std::runtime_error when the local coordinates would lose precision
// as float (|local| > scanner.max_local_coord):
//  - scanner position not given: the data is likely in a projected/survey CRS;
//    the user must pass --scanner.x0/y0/z0.
//  - scanner position given: the points are still too far from it; the
//    position is probably wrong.
// Emits warnings (not errors) for a far nearest point and suspicious extents.
LoadedCloud prepareCloud(const RawCloud& raw, const ScannerParams& sp);

std::string formatReport(const SanityReport& r, const RawCloud& raw);

}  // namespace cm
