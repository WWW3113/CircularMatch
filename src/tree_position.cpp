#include "cm/tree_position.hpp"

#include <cmath>

namespace cm {

std::optional<IntersectResult> axisDtmIntersection(const Eigen::Vector3d& p, const Eigen::Vector3d& v_in, const Dtm& dtm,
                                                   const IntersectParams& ip, double t_ref) {
  Eigen::Vector3d v = v_in.normalized();
  if (v.z() < 0) v = -v;
  if (v.z() < 1e-6) return std::nullopt;  // horizontal axis
  auto f = [&](double t) -> std::optional<double> {
    const Eigen::Vector3d x = p + t * v;
    const auto h = dtm.height(x.x(), x.y());
    if (!h) return std::nullopt;
    return x.z() - *h;
  };
  IntersectResult r;
  // 1. fixed point, starting from the DTM height below the reference point.
  double t = t_ref;
  for (int k = 0; k < ip.max_iter; ++k) {
    const Eigen::Vector3d x = p + t * v;
    const auto h = dtm.height(x.x(), x.y());
    if (!h) break;
    const double tn = (*h - p.z()) / v.z();
    r.iterations = k + 1;
    const auto fn = f(tn);
    if (fn && std::abs(*fn) <= ip.tol) {
      r.point = p + tn * v;
      return r;
    }
    if (!std::isfinite(tn) || std::abs(tn - t_ref) > ip.search_range) break;
    t = tn;
  }
  // 2. bracket scan + bisection.
  r.used_bisection = true;
  const int steps = static_cast<int>(std::ceil(ip.search_range / ip.scan_step));
  // Scan outward from t_ref so the bracket nearest the stem is found first.
  for (int k = 0; k < steps; ++k) {
    for (int sgn : {-1, 1}) {
      const double a = t_ref + sgn * k * ip.scan_step, b = t_ref + sgn * (k + 1) * ip.scan_step;
      const auto fa = f(a), fb = f(b);
      if (!fa || !fb || (*fa > 0) == (*fb > 0)) continue;
      double lo = a, hi = b, flo = *fa;
      for (int it = 0; it < 200; ++it) {
        const double mid = 0.5 * (lo + hi);
        const auto fm = f(mid);
        if (!fm) return std::nullopt;
        r.iterations++;
        if (std::abs(*fm) <= ip.tol || std::abs(hi - lo) < 1e-9) {
          r.point = p + mid * v;
          return r;
        }
        if ((*fm > 0) == (flo > 0)) { lo = mid; flo = *fm; }
        else hi = mid;
      }
    }
  }
  return std::nullopt;
}

}  // namespace cm
