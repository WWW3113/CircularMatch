#include "cm/synth.hpp"

#include <Eigen/Geometry>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

#include "cm/retention.hpp"  // splitmix64

namespace cm {

namespace {

// Portable deterministic RNG (std distributions are implementation-defined).
struct Rng {
  std::uint64_t s;
  double uni() {
    s = splitmix64(s);
    return static_cast<double>(s >> 11) * 0x1.0p-53;
  }
  double normal() {
    const double u1 = std::max(uni(), 1e-300), u2 = uni();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
  }
};

}  // namespace

SynthParams SynthParams::defaultScene() {
  SynthParams p;
  const double up = std::atan2(p.slope_y, p.slope_x) * 180.0 / M_PI;  // uphill azimuth
  p.stems = {
      {5.0, 2.0, 0.15, 0, 0},
      {-6.0, 4.0, 0.25, 0, 0},
      {3.0, -8.0, 0.08, 0, 0},
      {-4.0, -6.0, 0.20, 10.0, up},          // tilted uphill
      {9.0, 6.0, 0.30, 15.0, up + 180.0},    // tilted downhill
  };
  return p;
}

RawCloud makeSynthetic(const SynthParams& p, std::vector<SynthTruth>* truth) {
  Rng rng{p.seed};
  RawCloud rc;
  rc.format = "synthetic";
  auto ground = [&](double x, double y) { return p.ground_z0 + p.slope_x * x + p.slope_y * y; };
  auto keepProb = [&](double x, double y) {
    const double d = std::hypot(x, y);
    return d <= p.d_ref ? 1.0 : (p.d_ref / d) * (p.d_ref / d);
  };
  struct Axis {
    Eigen::Vector3d b, v, e1, e2;
    double r;
  };
  std::vector<Axis> axes;
  if (truth) truth->clear();
  for (const auto& s : p.stems) {
    Axis a;
    a.b = Eigen::Vector3d(s.x, s.y, ground(s.x, s.y));
    const double t = s.tilt_deg * M_PI / 180.0, az = s.tilt_az_deg * M_PI / 180.0;
    a.v = Eigen::Vector3d(std::sin(t) * std::cos(az), std::sin(t) * std::sin(az), std::cos(t));
    a.e1 = a.v.unitOrthogonal();
    a.e2 = a.v.cross(a.e1);
    a.r = s.radius;
    axes.push_back(a);
    if (truth) truth->push_back({a.b + p.offset, s.radius, s.tilt_deg});
  }
  // Ground.
  const double area = 4.0 * p.half_size * p.half_size;
  const auto n_ground = static_cast<std::size_t>(area * p.ground_density);
  for (std::size_t k = 0; k < n_ground; ++k) {
    const double x = (2 * rng.uni() - 1) * p.half_size, y = (2 * rng.uni() - 1) * p.half_size;
    const double keep = rng.uni();
    if (keep >= keepProb(x, y)) continue;
    bool inside = false;
    for (const auto& a : axes) inside |= std::hypot(x - a.b.x(), y - a.b.y()) < a.r;
    if (inside) continue;
    rc.points.emplace_back(x, y, ground(x, y) + p.noise_sigma * rng.normal());
  }
  // Stems.
  for (const auto& a : axes) {
    const double len = p.stem_height + 0.5;
    const auto n = static_cast<std::size_t>(2 * M_PI * a.r * len * p.stem_density);
    for (std::size_t k = 0; k < n; ++k) {
      const double t = -0.5 + len * rng.uni(), th = 2 * M_PI * rng.uni();
      const double keep = rng.uni();
      const Eigen::Vector3d nrm = std::cos(th) * a.e1 + std::sin(th) * a.e2;
      const Eigen::Vector3d q = a.b + t * a.v + a.r * nrm;
      if (q.z() < ground(q.x(), q.y())) continue;
      if (keep >= keepProb(q.x(), q.y())) continue;
      if (p.visible_half_only && nrm.dot(-q) <= 0) continue;  // scanner at origin
      rc.points.push_back(q + p.noise_sigma * Eigen::Vector3d(rng.normal(), rng.normal(), rng.normal()));
    }
  }
  for (auto& q : rc.points) q += p.offset;
  return rc;
}

void writeXyz(const std::string& path, const std::vector<Eigen::Vector3d>& pts) {
  std::ofstream o(path);
  if (!o) throw std::runtime_error("cannot write " + path);
  o.setf(std::ios::fixed);
  o.precision(4);
  for (const auto& q : pts) o << q.x() << " " << q.y() << " " << q.z() << "\n";
}

void writeLas(const std::string& path, const std::vector<Eigen::Vector3d>& pts, int minor, int format, double scale,
              const Eigen::Vector3d& off, int format_byte_override, std::uint16_t extra) {
  static const std::uint16_t kLen[11] = {20, 28, 26, 34, 57, 63, 30, 36, 38, 59, 67};
  const std::uint16_t hsize = minor <= 2 ? 227 : (minor == 3 ? 235 : 375);
  const std::uint16_t rlen = static_cast<std::uint16_t>((format >= 0 && format <= 10 ? kLen[format] : 20) + extra);
  std::vector<std::uint8_t> h(hsize, 0);
  std::memcpy(h.data(), "LASF", 4);
  h[24] = 1;
  h[25] = static_cast<std::uint8_t>(minor);
  auto put = [&](std::size_t o, const void* v, std::size_t n) { std::memcpy(h.data() + o, v, n); };
  put(94, &hsize, 2);
  const std::uint32_t off_pts = hsize;
  put(96, &off_pts, 4);
  h[104] = static_cast<std::uint8_t>(format_byte_override >= 0 ? format_byte_override : format);
  put(105, &rlen, 2);
  const std::uint32_t n32 = (minor >= 4 && format >= 6) ? 0u : static_cast<std::uint32_t>(pts.size());
  put(107, &n32, 4);
  const double sc[3] = {scale, scale, scale};
  put(131, sc, 24);
  put(155, off.data(), 24);
  if (minor >= 4) {
    const std::uint64_t n64 = pts.size();
    put(247, &n64, 8);
  }
  std::ofstream o(path, std::ios::binary);
  if (!o) throw std::runtime_error("cannot write " + path);
  o.write(reinterpret_cast<const char*>(h.data()), hsize);
  std::vector<std::uint8_t> rec(rlen, 0);
  for (const auto& q : pts) {
    std::fill(rec.begin(), rec.end(), 0xAB);  // junk in non-XYZ fields must be ignored
    for (int k = 0; k < 3; ++k) {
      const auto xi = static_cast<std::int32_t>(std::llround((q[k] - off[k]) / scale));
      std::memcpy(rec.data() + 4 * k, &xi, 4);
    }
    o.write(reinterpret_cast<const char*>(rec.data()), rlen);
  }
}

}  // namespace cm
