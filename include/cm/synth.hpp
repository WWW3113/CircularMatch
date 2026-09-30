// Synthetic TLS-like scene for tests: sloped ground plane + stems (cylinders),
// some tilted, with point density ~ min(1, (d_ref/d)^2) (d = horizontal
// distance to the scanner), optional visible-half-only stems and Gaussian noise.
// Ground truth tree position = axis x ground plane (exact).
#pragma once
#include <Eigen/Core>
#include <cstdint>
#include <vector>

#include "cm/io.hpp"

namespace cm {

struct SynthStem {
  double x, y;            // axis/ground intersection, scanner frame
  double radius;
  double tilt_deg = 0;    // axis tilt from vertical
  double tilt_az_deg = 0; // azimuth of the tilt direction (0 = +x)
};

struct SynthParams {
  double slope_x = 0.2, slope_y = 0.1;  // ground z = ground_z0 + slope_x x + slope_y y
  double ground_z0 = -1.5;              // scanner 1.5 m above ground at the origin
  double half_size = 15.0;              // scene is [-h, h]^2
  double ground_density = 4000;         // points / m^2 at d <= d_ref
  double stem_density = 20000;          // points / m^2 of stem surface at d <= d_ref
  double d_ref = 3.0;
  double stem_height = 4.5;             // along the axis above ground
  double noise_sigma = 0.003;
  bool visible_half_only = true;
  std::uint64_t seed = 42;
  Eigen::Vector3d offset = Eigen::Vector3d::Zero();  // added to all output coordinates (e.g. survey CRS)
  std::vector<SynthStem> stems;

  static SynthParams defaultScene();  // slope + 5 stems, two of them tilted (10 deg uphill, 15 deg downhill)
};

struct SynthTruth {
  Eigen::Vector3d position;  // with offset
  double radius;
  double tilt_deg;
};

RawCloud makeSynthetic(const SynthParams& p, std::vector<SynthTruth>* truth);

// Writers used by cm_synth and the I/O tests.
void writeXyz(const std::string& path, const std::vector<Eigen::Vector3d>& pts);
// LAS writer for tests: any version 1.0-1.4 / format 0-10; `format_byte_override`
// lets tests write invalid headers (e.g. compressed bit).
void writeLas(const std::string& path, const std::vector<Eigen::Vector3d>& pts, int minor, int format,
              double scale = 0.001, const Eigen::Vector3d& offset = Eigen::Vector3d::Zero(),
              int format_byte_override = -1, std::uint16_t extra_record_bytes = 0);

}  // namespace cm
