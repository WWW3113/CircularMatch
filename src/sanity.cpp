#include "cm/sanity.hpp"

#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cm {

LoadedCloud prepareCloud(const RawCloud& raw, const ScannerParams& sp) {
  if (raw.points.empty()) throw std::runtime_error("empty point cloud");
  const Eigen::Vector3d s(sp.x0, sp.y0, sp.z0);
  SanityReport rep;
  rep.n_points = raw.points.size();
  rep.bbox_min = Eigen::Vector3d::Constant(std::numeric_limits<double>::infinity());
  rep.bbox_max = -rep.bbox_min;
  double nearest2 = std::numeric_limits<double>::infinity();
  double max_abs = 0.0;
  for (const auto& p : raw.points) {
    rep.bbox_min = rep.bbox_min.cwiseMin(p);
    rep.bbox_max = rep.bbox_max.cwiseMax(p);
    const Eigen::Vector3d l = p - s;
    nearest2 = std::min(nearest2, l.x() * l.x() + l.y() * l.y());
    max_abs = std::max(max_abs, l.cwiseAbs().maxCoeff());
  }
  rep.nearest_horizontal = std::sqrt(nearest2);
  rep.max_abs_local = max_abs;

  if (max_abs > sp.max_local_coord) {
    std::ostringstream m;
    m.precision(12);
    m << "local coordinates reach " << max_abs << " m (limit scanner.max_local_coord = " << sp.max_local_coord
      << " m); converting to float would lose precision (1 cm voxels / hash quantum). bbox min = ("
      << rep.bbox_min.transpose() << "), max = (" << rep.bbox_max.transpose() << "). ";
    if (!sp.position_given)
      m << "The scanner position is still the default (0,0,0) while the data looks like a projected/survey "
           "coordinate system. Provide the real scanner position with --scanner.x0=... --scanner.y0=... "
           "--scanner.z0=... (original coordinates). Stopping.";
    else
      m << "The given scanner position (" << s.transpose()
        << ") is far from the data; check it (wrong CRS, swapped x/y, or wrong units?). Stopping.";
    throw std::runtime_error(m.str());
  }

  if (rep.nearest_horizontal > sp.nearest_warn) {
    std::ostringstream m;
    m << "nearest point is " << rep.nearest_horizontal << " m (horizontal) from the scanner position ("
      << s.transpose() << "), more than scanner.nearest_warn = " << sp.nearest_warn
      << " m. The cloud may not be centred on the scanner; d and P(d) would then be wrong.";
    rep.warnings.push_back(m.str());
  }
  const Eigen::Vector3d ext = rep.bbox_max - rep.bbox_min;
  if (ext.head<2>().maxCoeff() < 1.0)
    rep.warnings.push_back("horizontal extent < 1 m: are the units metres?");
  if (ext.maxCoeff() > 5000.0)
    rep.warnings.push_back("extent > 5 km: are the units metres (not mm/cm)?");
  if (ext.z() > 200.0)
    rep.warnings.push_back("vertical extent > 200 m: is z really vertical and in metres?");

  LoadedCloud out;
  out.offset = s;
  out.report = rep;
  out.cloud.reset(new Cloud);
  out.cloud->reserve(raw.points.size());
  for (const auto& p : raw.points) {
    const Eigen::Vector3d l = p - s;
    out.cloud->push_back(PointT(static_cast<float>(l.x()), static_cast<float>(l.y()), static_cast<float>(l.z())));
  }
  out.cloud->width = static_cast<std::uint32_t>(out.cloud->size());
  out.cloud->height = 1;
  out.cloud->is_dense = true;
  return out;
}

std::string formatReport(const SanityReport& r, const RawCloud& raw) {
  std::ostringstream o;
  o.setf(std::ios::fixed);
  o.precision(3);
  o << "input: " << r.n_points << " points, format " << raw.format;
  if (!raw.detail.empty()) o << " (" << raw.detail << ")";
  if (raw.skipped) o << ", skipped " << raw.skipped << " rows/points";
  o << "\n  bbox min (" << r.bbox_min.x() << ", " << r.bbox_min.y() << ", " << r.bbox_min.z() << ")"
    << "\n  bbox max (" << r.bbox_max.x() << ", " << r.bbox_max.y() << ", " << r.bbox_max.z() << ")"
    << "\n  nearest point to scanner (horizontal): " << r.nearest_horizontal << " m"
    << "\n  max |local coordinate|: " << r.max_abs_local << " m";
  for (const auto& w : r.warnings) o << "\n  WARNING: " << w;
  return o.str();
}

}  // namespace cm
