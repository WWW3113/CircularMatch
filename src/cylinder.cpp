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
  if (p.lm_refit && inliers.size() > 7) {
    Eigen::VectorXf refined;
    model->optimizeModelCoefficients(inliers, coef, refined);
    if (refined.size() == 7 && refined.allFinite()) {
      coef = refined;
      model->selectWithinDistance(coef, p.dist_threshold, inliers);
    }
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
  r.n_inliers = inliers.size();

  const double ratio = double(r.n_inliers) / double(r.n_points);
  if (ratio < p.min_inlier_ratio) r.fail_mask |= 1u << int(FitFail::LowInlierRatio);
  if (r.radius < p.radius_min || r.radius > p.radius_max) r.fail_mask |= 1u << int(FitFail::RadiusOutOfRange);
  if (r.tilt_deg > p.max_tilt_deg) r.fail_mask |= 1u << int(FitFail::TiltTooLarge);
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
