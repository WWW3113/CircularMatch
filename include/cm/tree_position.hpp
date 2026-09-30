// Tree position = intersection of the cylinder axis with the DTM surface.
// Axis: x(t) = p + t v. Solve f(t) = p_z + t v_z - DTM(p_x + t v_x, p_y + t v_y) = 0.
// For a tilted axis the DTM height must be taken along the axis, not at (p_x, p_y).
//  1. fixed-point iteration t <- (DTM(x(t), y(t)) - p_z) / v_z  (converges when
//     |terrain slope along v_h * tan(tilt)| < 1; the usual case);
//  2. if it does not converge: scan t in [t_ref - R, t_ref + R] for a sign
//     change of f and bisect.
#pragma once
#include <Eigen/Core>
#include <optional>

#include "cm/config.hpp"
#include "cm/dtm.hpp"

namespace cm {

struct IntersectResult {
  Eigen::Vector3d point;
  int iterations = 0;
  bool used_bisection = false;
};

// t_ref: axis parameter to centre the bracket search on (e.g. projection of
// the cluster centroid). Returns nullopt if no intersection within the DTM.
std::optional<IntersectResult> axisDtmIntersection(const Eigen::Vector3d& p, const Eigen::Vector3d& v, const Dtm& dtm,
                                                   const IntersectParams& ip, double t_ref = 0.0);

}  // namespace cm
