#include "cm/cylinder.hpp"

#include <pcl/sample_consensus/ransac.h>
#include <pcl/sample_consensus/sac_model_cylinder.h>

#include <cmath>

#include "cm/retention.hpp"  // splitmix64

namespace cm {

namespace {

class SeededCylinderModel : public pcl::SampleConsensusModelCylinder<PointT, NormalT> {
 public:
  using Base = pcl::SampleConsensusModelCylinder<PointT, NormalT>;
  using Ptr = std::shared_ptr<SeededCylinderModel>;
  explicit SeededCylinderModel(const Cloud::ConstPtr& c) : Base(c, /*random=*/false) {}
  // rng_gen_ holds a reference to rng_alg_, so this re-seeds all sampling.
  void reseed(std::uint32_t s) { this->rng_alg_.seed(s); }
  using Base::getSamples;
};

struct Sub {
  Cloud::Ptr cloud{new Cloud};
  NormalCloud::Ptr normals{new NormalCloud};
};

Sub extract(const Cloud& cloud, const NormalCloud& normals, const std::vector<int>& idx) {
  Sub s;
  s.cloud->reserve(idx.size());
  s.normals->reserve(idx.size());
  for (int i : idx) {
    s.cloud->push_back(cloud[i]);
    s.normals->push_back(normals[i]);
  }
  return s;
}

}  // namespace

const char* toString(FitFail f) {
  switch (f) {
    case FitFail::None: return "ok";
    case FitFail::TooFewPoints: return "too_few_points";
    case FitFail::NoModel: return "no_model";
    case FitFail::LowInlierRatio: return "low_inlier_ratio";
    case FitFail::RadiusOutOfRange: return "radius_out_of_range";
    case FitFail::TiltTooLarge: return "tilt_too_large";
    case FitFail::NormalInconsistent: return "normal_inconsistent";
    case FitFail::ArcCoverageLow: return "arc_coverage_low";
    case FitFail::NoDtmIntersection: return "no_dtm_intersection";
    default: return "?";
  }
}

std::uint32_t clusterSeed(std::uint64_t base, const Cloud& cloud, const std::vector<int>& idx) {
  double cx = 0, cy = 0, cz = 0;
  for (int i : idx) { cx += cloud[i].x; cy += cloud[i].y; cz += cloud[i].z; }
  const double n = idx.empty() ? 1.0 : double(idx.size());
  auto q = [](double v) { return static_cast<std::uint64_t>(std::llround(v / 1e-3)); };
  std::uint64_t h = splitmix64(base ^ 0x5EEDC71DULL);
  h = splitmix64(h ^ q(cx / n));
  h = splitmix64(h ^ q(cy / n));
  h = splitmix64(h ^ q(cz / n));
  return static_cast<std::uint32_t>(h >> 32);
}

CylinderFit fitCylinder(const Cloud& cloud, const NormalCloud& normals, const std::vector<int>& idx,
                        const CylinderParams& p, std::uint32_t ransac_seed) {
  CylinderFit r;
  r.n_points = idx.size();
  r.ransac_seed = ransac_seed;
  for (int i : idx) r.centroid += cloud[i].getVector3fMap().cast<double>();
  if (!idx.empty()) r.centroid /= double(idx.size());
  if (static_cast<int>(idx.size()) < std::max(p.min_points, 3)) {
    r.fail = FitFail::TooFewPoints;
    r.fail_mask = 1u << int(FitFail::TooFewPoints);
    return r;
  }
  const Sub s = extract(cloud, normals, idx);
  SeededCylinderModel::Ptr model(new SeededCylinderModel(s.cloud));
  model->setInputNormals(s.normals);
  model->setNormalDistanceWeight(p.normal_weight);
  model->reseed(ransac_seed);

  pcl::RandomSampleConsensus<PointT> sac(model, p.dist_threshold);
  sac.setMaxIterations(p.max_iterations);
  sac.setProbability(p.probability);
  sac.setNumberOfThreads(-1);  // no OpenMP inside RANSAC: keeps sampling order deterministic
  if (!sac.computeModel()) {
    r.fail = FitFail::NoModel;
    r.fail_mask = 1u << int(FitFail::NoModel);
    return r;
  }
  pcl::Indices inliers;
  sac.getInliers(inliers);
  Eigen::VectorXf coef;
  sac.getModelCoefficients(coef);
  if (coef.size() != 7) {
    r.fail = FitFail::NoModel;
    r.fail_mask = 1u << int(FitFail::NoModel);
    return r;
  }
  // LM refit (PCL's optimizeModelCoefficients: Euclidean point-to-surface
  // distance) on the GEOMETRIC inliers of the current model. Using PCL's
  // normal-weighted inliers here would drop points with poor normals and bias
  // the arc that constrains the radius. Repeated lm_passes times.
  auto geometricInliers = [&](const Eigen::VectorXf& c) {
    pcl::Indices out;
    Eigen::Vector3d ap(c[0], c[1], c[2]), d(c[3], c[4], c[5]);
    if (d.norm() < 1e-12) return out;
    d.normalize();
    for (std::size_t k = 0; k < s.cloud->size(); ++k) {
      const Eigen::Vector3d w = (*s.cloud)[k].getVector3fMap().cast<double>() - ap;
      if (std::abs((w - w.dot(d) * d).norm() - std::abs(double(c[6]))) <= p.dist_threshold) out.push_back(int(k));
    }
    return out;
  };
  if (p.lm_refit) {
    for (int pass = 0; pass < std::max(1, p.lm_passes); ++pass) {
      const pcl::Indices gi = geometricInliers(coef);
      if (gi.size() <= 7) break;
      Eigen::VectorXf refined;
      model->optimizeModelCoefficients(gi, coef, refined);
      if (refined.size() != 7 || !refined.allFinite()) break;
      coef = refined;
    }
    model->selectWithinDistance(coef, p.dist_threshold, inliers);
  }
  Eigen::Vector3d dir(coef[3], coef[4], coef[5]);
  if (dir.norm() < 1e-12) {
    r.fail = FitFail::NoModel;
    r.fail_mask = 1u << int(FitFail::NoModel);
    return r;
  }
  dir.normalize();
  if (dir.z() < 0) dir = -dir;
  r.axis_point = Eigen::Vector3d(coef[0], coef[1], coef[2]);
  r.axis_dir = dir;
  r.radius = std::abs(coef[6]);
  r.tilt_deg = std::acos(std::min(1.0, dir.z())) * 180.0 / M_PI;
  r.n_inliers_sac = inliers.size();
  const double cos_max = std::cos(p.normal_max_angle_deg * M_PI / 180.0);
  const Eigen::Vector3d u1 = dir.unitOrthogonal(), u2 = dir.cross(u1);
  constexpr int kArcBins = 72;  // 5 degrees
  std::vector<char> arc(kArcBins, 0);
  for (std::size_t k = 0; k < s.cloud->size(); ++k) {
    const Eigen::Vector3d w = (*s.cloud)[k].getVector3fMap().cast<double>() - r.axis_point;
    const Eigen::Vector3d radial = w - w.dot(dir) * dir;  // surface normal direction at q
    const double dist_axis = radial.norm();
    if (std::abs(dist_axis - r.radius) <= p.dist_threshold) {
      ++r.n_inliers;
      const double ang = std::atan2(radial.dot(u2), radial.dot(u1)) + M_PI;  // [0, 2pi]
      arc[std::min(kArcBins - 1, static_cast<int>(ang / (2 * M_PI) * kArcBins))] = 1;
    }
    const Eigen::Vector3d nq((*s.normals)[k].normal_x, (*s.normals)[k].normal_y, (*s.normals)[k].normal_z);
    if (dist_axis > 1e-12 && nq.allFinite() && nq.norm() > 1e-12)
      r.n_normal_ok += std::abs(nq.dot(radial) / (dist_axis * nq.norm())) >= cos_max ? 1 : 0;
  }

  for (char b : arc) r.arc_deg += b ? 360.0 / kArcBins : 0.0;
  const double ratio = double(r.n_inliers) / double(r.n_points);
  if (ratio < p.min_inlier_ratio) r.fail_mask |= 1u << int(FitFail::LowInlierRatio);
  if (r.radius < p.radius_min || r.radius > p.radius_max) r.fail_mask |= 1u << int(FitFail::RadiusOutOfRange);
  if (r.tilt_deg > p.max_tilt_deg) r.fail_mask |= 1u << int(FitFail::TiltTooLarge);
  // Our additions (not in the paper): metrics are always computed and written
  // to the diagnostics CSV; they only reject a fit when enabled.
  if (p.check_normal_consistency && double(r.n_normal_ok) / double(r.n_points) < p.min_normal_ratio)
    r.fail_mask |= 1u << int(FitFail::NormalInconsistent);
  if (p.check_arc_coverage && r.arc_deg < p.min_arc_deg) r.fail_mask |= 1u << int(FitFail::ArcCoverageLow);
  for (int f = 1; f < int(FitFail::Count); ++f)
    if (r.fail_mask & (1u << f)) {
      r.fail = FitFail(f);
      break;
    }
  return r;
}

std::vector<std::vector<int>> drawSamplesForTest(const Cloud& cloud, const NormalCloud& normals, std::uint32_t seed,
                                                 int n) {
  Cloud::Ptr c(new Cloud(cloud));
  NormalCloud::Ptr nc(new NormalCloud(normals));
  SeededCylinderModel m(c);
  m.setInputNormals(nc);
  m.reseed(seed);
  std::vector<std::vector<int>> out;
  for (int k = 0; k < n; ++k) {
    int it = 0;
    pcl::Indices s;
    m.getSamples(it, s);
    out.emplace_back(s.begin(), s.end());
  }
  return out;
}

}  // namespace cm
