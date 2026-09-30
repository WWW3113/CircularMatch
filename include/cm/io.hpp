// Point cloud input. Coordinates are read as double in the file's original
// coordinate system (metres); conversion to the float, scanner-centred local
// frame happens in sanity.hpp (prepareCloud), after the large-coordinate check.
#pragma once
#include <Eigen/Core>
#include <string>
#include <vector>

namespace cm {

struct RawCloud {
  std::vector<Eigen::Vector3d> points;  // original coordinates [m]
  std::string format;                   // "xyz", "pcd", "ply", "las"
  std::size_t skipped = 0;              // unparsable lines / non-finite points
  std::string detail;                   // e.g. "LAS 1.2 format 3"
};

// Dispatches on the (case-insensitive) extension:
//   .xyz .txt .csv  ASCII, first three numeric columns = x y z (header/comment lines skipped)
//   .pcd            ASCII / binary / binary_compressed via PCL (float or double x,y,z fields)
//   .ply            ASCII / binary via PCL
//   .las            uncompressed LAS 1.0-1.4, point formats 0-10, XYZ only (own reader)
//   .laz            rejected: decompress to LAS first (e.g. `laszip -i in.laz -o out.las`)
// Throws std::runtime_error on any unsupported or malformed input.
RawCloud readPointCloud(const std::string& path);

// Own minimal LAS reader (exposed for tests).
RawCloud readLas(const std::string& path);

// Reference tree positions: ASCII, first two or three numeric columns x y [z],
// original coordinates. Non-numeric lines are skipped.
std::vector<Eigen::Vector3d> readReferencePositions(const std::string& path);

}  // namespace cm
