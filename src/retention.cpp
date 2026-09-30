#include "cm/retention.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>

namespace cm {

RetentionKind parseRetentionKind(const std::string& s) {
  if (s == "step") return RetentionKind::Step;
  if (s == "linear_a") return RetentionKind::LinearA;
  if (s == "linear_mid") return RetentionKind::LinearMid;
  if (s == "physical") return RetentionKind::Physical;
  throw std::runtime_error("unknown retention version '" + s + "' (step | linear_a | linear_mid | physical)");
}

std::string toString(RetentionKind k) {
  switch (k) {
    case RetentionKind::Step: return "step";
    case RetentionKind::LinearA: return "linear_a";
    case RetentionKind::LinearMid: return "linear_mid";
    case RetentionKind::Physical: return "physical";
  }
  return "?";
}

double RetentionModel::operator()(double d) const {
  double p = 0;
  switch (kind) {
    case RetentionKind::Step:
      p = d < 5.0 ? 0.5 : (d < 10.0 ? 0.75 : 1.0);
      break;
    case RetentionKind::LinearA:
    case RetentionKind::LinearMid:
      p = intercept + slope * d;
      break;
    case RetentionKind::Physical: {
      const double r = d / d0;
      p = std::max(p_min, r * r);
      break;
    }
  }
  return std::clamp(p, 0.0, 1.0);
}

RetentionModel RetentionModel::make(RetentionKind k, const RetentionParams& p) {
  RetentionModel m;
  m.kind = k;
  m.slope = p.linear_slope;
  m.d0 = p.physical_d0;
  m.p_min = p.physical_pmin;
  m.intercept = k == RetentionKind::LinearMid ? 0.375 : 0.5;
  m.label = toString(k);
  return m;
}

std::string RetentionModel::describe() const {
  std::ostringstream o;
  o.precision(6);
  switch (kind) {
    case RetentionKind::Step: o << "step: d<5:0.5, 5<=d<10:0.75, d>=10:1"; break;
    case RetentionKind::LinearA:
    case RetentionKind::LinearMid: o << label << ": clamp(" << intercept << " + " << slope << " d, 0, 1)"; break;
    case RetentionKind::Physical: o << label << ": clip((d/" << d0 << ")^2, " << p_min << ", 1)"; break;
  }
  return o.str();
}

std::uint64_t splitmix64(std::uint64_t x) {
  x += 0x9E3779B97F4A7C15ULL;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
  return x ^ (x >> 31);
}

double commonUniform(std::uint64_t seed, const PointT& p, double quantum) {
  auto q = [&](float v) { return static_cast<std::uint64_t>(std::llround(double(v) / quantum)); };
  std::uint64_t h = splitmix64(seed);
  h = splitmix64(h ^ q(p.x));
  h = splitmix64(h ^ q(p.y));
  h = splitmix64(h ^ q(p.z));
  return static_cast<double>(h >> 11) * 0x1.0p-53;
}

std::vector<RetentionBin> retentionBins(const std::vector<double>& d, const std::vector<char>& kept,
                                        const RetentionModel& m, double bin) {
  std::vector<RetentionBin> bins;
  double dmax = 0;
  for (double x : d) dmax = std::max(dmax, x);
  const int nb = static_cast<int>(std::floor(dmax / bin)) + 1;
  bins.resize(nb);
  for (int b = 0; b < nb; ++b) {
    bins[b].d_lo = b * bin;
    bins[b].d_hi = (b + 1) * bin;
  }
  for (std::size_t i = 0; i < d.size(); ++i) {
    auto& b = bins[static_cast<int>(std::floor(d[i] / bin))];
    ++b.n;
    b.kept += kept[i] ? 1 : 0;
    b.p_mean += m(d[i]);
  }
  for (auto& b : bins)
    if (b.n) b.p_mean /= double(b.n);
  return bins;
}

}  // namespace cm
