#include "cm/pipeline.hpp"

#include "cm/cluster.hpp"
#include "cm/tree_position.hpp"
#include "cm/voxel.hpp"

namespace cm {

Preprocessed preprocess(const LoadedCloud& in, const Config& cfg) {
  Preprocessed pre;
  pre.offset = in.offset;
  const Cloud& cloud = *in.cloud;
  pre.stages.push_back({"0 input", cloud.size(), 0});

  Timer t1;
  pre.dtm = Dtm::build(cloud, cfg.dtm);
  const auto hf = heightFilter(cloud, pre.dtm, cfg.height, &pre.height_no_dtm);
  pre.stages.push_back({"1 dtm+height", hf.size(), t1.ms()});

  Timer t2;
  const auto vox = voxelNearestToCentroid(cloud, hf, cfg.voxel.leaf);
  pre.voxel.reset(new Cloud);
  pre.voxel->reserve(vox.size());
  for (int i : vox) pre.voxel->push_back(cloud[i]);
  pre.stages.push_back({"2 voxel", pre.voxel->size(), t2.ms()});

  Timer t3;
  NormalResult nr = computeNormals(*pre.voxel, cfg.normals);
  pre.normals = nr.normals;
  pre.normals_invalid = nr.n_invalid;
  pre.stages.push_back({"3 normals", pre.voxel->size() - nr.n_invalid, t3.ms()});

  Timer t4;
  for (std::size_t i = 0; i < pre.voxel->size(); ++i)
    if (nr.valid[i] && nr.verticality[i] > cfg.normals.vert_threshold) pre.vertical.push_back(int(i));
  pre.dist.reserve(pre.vertical.size());
  for (int i : pre.vertical) pre.dist.push_back(scannerDistance((*pre.voxel)[i], cfg.scanner.use_3d_distance));
  pre.stages.push_back({"4 verticality", pre.vertical.size(), t4.ms()});
  return pre;
}

double RunResult::total_ms() const {
  double s = 0;
  for (const auto& st : stages) s += st.ms;
  return s;
}

std::vector<double> computeUniforms(const Preprocessed& pre, std::uint64_t seed, const Config& cfg) {
  std::vector<double> u(pre.vertical.size());
  for (std::size_t k = 0; k < pre.vertical.size(); ++k)
    u[k] = commonUniform(seed, (*pre.voxel)[pre.vertical[k]], cfg.retention.hash_quantum);
  return u;
}

RunResult runStages(const Preprocessed& pre, const RetentionModel& model, std::uint64_t seed, const Config& cfg,
                    const std::vector<double>* uniforms, std::vector<char>* kept_flags) {
  RunResult r;
  r.label = model.label;
  r.seed = seed;
  r.model = model;
  r.candidates = pre.vertical.size();

  // 5. Bernoulli retention with common random numbers.
  Timer t5;
  std::vector<double> own;
  if (!uniforms) {
    own = computeUniforms(pre, seed, cfg);
    uniforms = &own;
  }
  std::vector<int> kept;
  if (kept_flags) kept_flags->assign(pre.vertical.size(), 0);
  for (std::size_t k = 0; k < pre.vertical.size(); ++k)
    if ((*uniforms)[k] < model(pre.dist[k])) {
      kept.push_back(pre.vertical[k]);
      if (kept_flags) (*kept_flags)[k] = 1;
    }
  r.kept = kept.size();
  r.stages.push_back({"5 retention", kept.size(), t5.ms()});

  // 6. clustering
  Timer t6;
  const auto clusters = euclideanClusters(*pre.voxel, kept, cfg.cluster);
  r.clusters = clusters.size();
  r.stages.push_back({"6 clustering", clusters.size(), t6.ms()});

  // 7. RANSAC cylinder
  Timer t7;
  const std::uint64_t base = cfg.cylinder.seed_mode == "fixed" ? cfg.cylinder.fixed_seed : seed;
  if (cfg.cylinder.seed_mode != "fixed" && cfg.cylinder.seed_mode != "follow")
    throw std::runtime_error("cylinder.seed_mode must be follow or fixed");
  std::vector<CylinderFit> fits(clusters.size());
  for (std::size_t c = 0; c < clusters.size(); ++c)
    fits[c] = fitCylinder(*pre.voxel, *pre.normals, clusters[c], cfg.cylinder,
                          clusterSeed(base, *pre.voxel, clusters[c]));
  std::size_t n_ok = 0;
  for (const auto& f : fits) n_ok += f.fail == FitFail::None;
  r.stages.push_back({"7 ransac", n_ok, t7.ms()});

  // 8. tree position = axis x DTM
  Timer t8;
  for (std::size_t c = 0; c < clusters.size(); ++c) {
    ClusterDiag dg;
    dg.cluster_id = int(c);
    dg.fit = fits[c];
    if (dg.fit.fail == FitFail::None) {
      Eigen::Vector3d cen = Eigen::Vector3d::Zero();
      for (int i : clusters[c]) cen += (*pre.voxel)[i].getVector3fMap().cast<double>();
      cen /= double(clusters[c].size());
      const double t_ref = (cen - dg.fit.axis_point).dot(dg.fit.axis_dir);
      const auto ix = axisDtmIntersection(dg.fit.axis_point, dg.fit.axis_dir, pre.dtm, cfg.intersect, t_ref);
      if (ix) {
        dg.has_position = true;
        TreeRecord tr;
        tr.cluster_id = int(c);
        tr.position = ix->point;
        tr.radius = dg.fit.radius;
        tr.tilt_deg = dg.fit.tilt_deg;
        tr.n_points = dg.fit.n_points;
        tr.n_inliers = dg.fit.n_inliers;
        r.trees.push_back(tr);
      } else {
        dg.fit.fail = FitFail::NoDtmIntersection;
        dg.fit.fail_mask |= 1u << int(FitFail::NoDtmIntersection);
      }
    }
    r.fail_counts[int(dg.fit.fail)]++;
    r.diag.push_back(dg);
  }
  r.stages.push_back({"8 position", r.trees.size(), t8.ms()});
  return r;
}

}  // namespace cm
