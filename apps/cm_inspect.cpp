// Inspect a point cloud file with the project's own reader (no pipeline run).
//   cm_inspect --input=scan.ply [--origin=x,y,z] [--stride=10] [--bin_deg=0.002]
//
// Reports: point count (and, for PLY, the header vertex count), skipped
// points, XYZ bounding box, ranges relative to the candidate origin, and an
// ANGULAR-LATTICE test of the candidate origin.
//
// Angular-lattice test: a TLS scanner samples on a regular azimuth/elevation
// grid around its optical centre. Seen from the true centre, the point angles
// fall onto that grid, so a fine angle histogram is "spiky"; seen from any
// other point the angles smear. The score is the share of points that fall in
// the 10% most populated histogram bins (bin width bin_deg, points with 3-D
// range 2-30 m). The score is printed for the candidate origin and for origins
// offset by +-2 cm, +-5 cm, +-20 cm and +1 m along each axis, and for the scan
// centroid. This is EVIDENCE about where
// the scanner was; it is not metadata.
#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "app_common.hpp"
#include "cm/io.hpp"

using namespace cm;

namespace {

long long plyHeaderVertexCount(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  std::string line;
  for (int k = 0; k < 200 && std::getline(in, line); ++k) {
    if (line.rfind("element vertex", 0) == 0) return std::stoll(line.substr(15));
    if (line.rfind("end_header", 0) == 0) break;
  }
  return -1;
}

struct LatticeScore {
  std::size_t n = 0;
  double elev = 0, azim = 0;
};

double topShare(const std::unordered_map<long long, int>& h, std::size_t n) {
  std::vector<int> v;
  v.reserve(h.size());
  for (const auto& kv : h) v.push_back(kv.second);
  std::sort(v.begin(), v.end(), std::greater<int>());
  const std::size_t k = std::max<std::size_t>(1, v.size() / 10);
  double s = 0;
  for (std::size_t i = 0; i < k; ++i) s += v[i];
  return n ? s / double(n) : 0.0;
}

LatticeScore lattice(const std::vector<Eigen::Vector3d>& pts, const Eigen::Vector3d& o, int stride, double bin_deg) {
  std::unordered_map<long long, int> he, ha;
  LatticeScore s;
  const double k = 180.0 / M_PI / bin_deg;
  for (std::size_t i = 0; i < pts.size(); i += stride) {
    const Eigen::Vector3d d = pts[i] - o;
    const double hr = std::hypot(d.x(), d.y()), r = d.norm();
    if (r < 2.0 || r > 30.0) continue;
    ++s.n;
    ++he[static_cast<long long>(std::floor(std::atan2(d.z(), hr) * k))];
    ++ha[static_cast<long long>(std::floor(std::atan2(d.y(), d.x()) * k))];
  }
  s.elev = topShare(he, s.n);
  s.azim = topShare(ha, s.n);
  return s;
}

double pct(std::vector<double>& v, double p) {
  if (v.empty()) return std::nan("");
  const std::size_t k = std::min(v.size() - 1, static_cast<std::size_t>(p / 100.0 * double(v.size() - 1)));
  std::nth_element(v.begin(), v.begin() + k, v.end());
  return v[k];
}

}  // namespace

int main(int argc, char** argv) {
  try {
    auto a = app::parseArgs(argc, argv, {"input", "origin", "stride", "bin_deg", "help"});
    if (a.opts.count("help") || !a.opts.count("input")) {
      std::cout << "usage: cm_inspect --input=FILE [--origin=x,y,z] [--stride=10] [--bin_deg=0.002]\n";
      return a.opts.count("help") ? 0 : 2;
    }
    const std::string path = a.opts["input"];
    Eigen::Vector3d o = Eigen::Vector3d::Zero();
    if (a.opts.count("origin")) {
      std::stringstream ss(a.opts["origin"]);
      char c;
      if (!(ss >> o.x() >> c >> o.y() >> c >> o.z())) throw std::runtime_error("--origin must be x,y,z");
    }
    const int stride = a.opts.count("stride") ? std::stoi(a.opts["stride"]) : 10;
    const double bin_deg = a.opts.count("bin_deg") ? std::stod(a.opts["bin_deg"]) : 0.002;

    const RawCloud rc = readPointCloud(path);
    std::printf("file: %s\nformat: %s %s\npoints read: %zu, skipped (non-finite/unparsable): %zu\n", path.c_str(),
                rc.format.c_str(), rc.detail.c_str(), rc.points.size(), rc.skipped);
    if (rc.format == "ply") {
      const long long hv = plyHeaderVertexCount(path);
      std::printf("PLY header vertex count: %lld -> %s\n", hv,
                  hv == static_cast<long long>(rc.points.size() + rc.skipped) ? "matches read+skipped"
                                                                              : "MISMATCH");
    }
    Eigen::Vector3d mn = Eigen::Vector3d::Constant(INFINITY), mx = -mn;
    std::vector<double> hr, r3;
    hr.reserve(rc.points.size());
    r3.reserve(rc.points.size());
    std::size_t in05 = 0, in1 = 0, in2 = 0;
    for (const auto& p : rc.points) {
      mn = mn.cwiseMin(p);
      mx = mx.cwiseMax(p);
      const Eigen::Vector3d d = p - o;
      const double h = std::hypot(d.x(), d.y());
      hr.push_back(h);
      r3.push_back(d.norm());
      in05 += h < 0.5;
      in1 += h < 1.0;
      in2 += h < 2.0;
    }
    std::printf("bbox x [%.3f, %.3f]  y [%.3f, %.3f]  z [%.3f, %.3f]  extent %.1f x %.1f x %.1f m\n", mn.x(), mx.x(),
                mn.y(), mx.y(), mn.z(), mx.z(), mx.x() - mn.x(), mx.y() - mn.y(), mx.z() - mn.z());
    std::printf("candidate origin (%.3f, %.3f, %.3f):\n", o.x(), o.y(), o.z());
    std::printf("  horizontal distance: min %.3f  p1 %.2f  median %.2f  p99 %.1f  max %.1f m\n",
                *std::min_element(hr.begin(), hr.end()), pct(hr, 1), pct(hr, 50), pct(hr, 99),
                *std::max_element(hr.begin(), hr.end()));
    std::printf("  3-D range: median %.2f  max %.1f m\n", pct(r3, 50), *std::max_element(r3.begin(), r3.end()));
    std::printf("  points within horizontal 0.5 / 1 / 2 m: %zu / %zu / %zu\n", in05, in1, in2);

    std::printf("angular-lattice test (stride %d, bin %.4f deg, share of points in top-10%% bins):\n", stride, bin_deg);
    std::printf("  %-26s %10s %10s %10s\n", "origin offset [m]", "n", "elevation", "azimuth");
    const double offs[] = {-0.2, -0.05, -0.02, 0.02, 0.05, 0.2, 1.0};
    std::vector<std::pair<std::string, Eigen::Vector3d>> cands = {{"0 (candidate)", Eigen::Vector3d::Zero()}};
    for (double d : offs)
      for (int ax = 0; ax < 3; ++ax) {
        Eigen::Vector3d v = Eigen::Vector3d::Zero();
        v[ax] = d;
        char name[64];
        std::snprintf(name, sizeof(name), "%c %+.2f", "xyz"[ax], d);
        cands.emplace_back(name, v);
      }
    Eigen::Vector3d cen = Eigen::Vector3d::Zero();
    for (const auto& p : rc.points) cen += p;
    cen /= double(rc.points.size());
    cands.emplace_back("scan centroid (absolute)", cen - o);
    std::printf("  (range clip check) points with 3-D range in [49.5, 50.0] m: %zu, > 50.0 m: %zu\n",
                std::count_if(r3.begin(), r3.end(), [](double v) { return v >= 49.5 && v <= 50.0; }),
                std::count_if(r3.begin(), r3.end(), [](double v) { return v > 50.0; }));
    for (const auto& c : cands) {
      const LatticeScore s = lattice(rc.points, o + c.second, stride, bin_deg);
      std::printf("  %-26s %10zu %10.3f %10.3f\n", c.first.c_str(), s.n, s.elev, s.azim);
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
