// Coarse registration of one scan pair from tree keypoints with the CN
// descriptor (paper Section II-A/B), evaluated against a ground-truth transform.
//
//   cm_register --src_trees=A_trees.csv --tgt_trees=B_trees.csv --src_cloud=A.ply --gt=A-B.tfm
//               [--thresholds=0.05,0.10,0.15] [--label=name] [--out=result.csv]
//               [--N=183 --w=10 --th_score=40 --min_dc=3] [--merge_radius=0]
//               [--ransac_iters=1000 --ransac_inlier=0.3 --ransac_seed=1] [--gt_match=0.3]
//   cm_register --batch=jobs.txt [same options]
//     jobs.txt: one job per line "src_trees tgt_trees src_cloud gt out label" (whitespace separated);
//     the source cloud is read once and reused while consecutive jobs share it (sort jobs by cloud).
//
// Tree CSVs: header with columns x, y, z (cm_extract / cm_compare *_trees.csv).
// GT .tfm: 4x4 matrix mapping source coordinates into the target frame.
// One output row per (threshold, solver). threshold 0.05 = paper setting;
// other thresholds are a sensitivity analysis (NOT the paper's setting).
#include <Eigen/Core>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "app_common.hpp"
#include "cm/cn_descriptor.hpp"
#include "cm/cn_matching.hpp"
#include "cm/io.hpp"
#include "cm/registration.hpp"
#include "cm/types.hpp"

using namespace cm;

namespace {

std::vector<Eigen::Vector3d> readTreesCsv(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  std::string line;
  std::getline(in, line);
  std::vector<std::string> head;
  {
    std::stringstream ss(line);
    std::string c;
    while (std::getline(ss, c, ',')) head.push_back(c);
  }
  int ix = -1, iy = -1, iz = -1;
  for (int k = 0; k < int(head.size()); ++k) {
    if (head[k] == "x") ix = k;
    if (head[k] == "y") iy = k;
    if (head[k] == "z") iz = k;
  }
  if (ix < 0 || iy < 0 || iz < 0) throw std::runtime_error(path + ": need columns x, y, z");
  std::vector<Eigen::Vector3d> out;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    std::vector<std::string> f;
    std::stringstream ss(line);
    std::string c;
    while (std::getline(ss, c, ',')) f.push_back(c);
    out.emplace_back(std::stod(f.at(ix)), std::stod(f.at(iy)), std::stod(f.at(iz)));
  }
  return out;
}

RigidTransform readTfm(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  double m[16];
  for (double& v : m)
    if (!(in >> v)) throw std::runtime_error(path + ": expected 16 numbers");
  RigidTransform T;
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 3; ++c) T.R(r, c) = m[4 * r + c];
    T.t(r) = m[4 * r + 3];
  }
  return T;
}

std::vector<double> parseList(const std::string& s) {
  std::vector<double> v;
  std::stringstream ss(s);
  std::string c;
  while (std::getline(ss, c, ',')) v.push_back(std::stod(c));
  return v;
}

struct Job {
  std::string src_trees, tgt_trees, src_cloud, gt, out, label;
};

struct Settings {
  CnParams base;
  double merge = 0, gt_match = 0.3;
  RansacParams rp;
  std::vector<double> thresholds;
};

// Runs one pair; returns the CSV text (header + one row per threshold and solver).
std::string runJob(const Job& job, const Settings& st, const RawCloud& cloud) {
  const CnParams& base = st.base;
  const double merge = st.merge, gt_match = st.gt_match;
  const RansacParams& rp = st.rp;
  const auto& thresholds = st.thresholds;
  auto skp = mergeKeypoints(readTreesCsv(job.src_trees), merge);
  auto tkp = mergeKeypoints(readTreesCsv(job.tgt_trees), merge);
  const RigidTransform gt = readTfm(job.gt);

  std::ostringstream o;
  o << std::setprecision(10);
  o << "label,threshold,setting,solver,n_src_kp,n_tgt_kp,descriptors_src,descriptors_tgt,d12_enc2_s0_is_p1,"
       "d12_enc3_s0_is_p1p2,gate_passed,candidates,best_score,best_dc,best_fm,used_triangle,tri_set_src,"
       "tri_set_tgt,tri_matches,n_matches,n_correct_matches,ransac_inliers,transform_ok,rot_err_deg,trans_err_m,"
       "e_p_m,success,time_ms,r00,r01,r02,r10,r11,r12,r20,r21,r22,t0,t1,t2\n";
  for (double thr : thresholds) {
    CnParams p = base;
    p.threshold = thr;
    Timer tm;
    CnStats ss, ts;
    const auto sd = buildCnDescriptors(skp, p, &ss);
    const auto td = buildCnDescriptors(tkp, p, &ts);
    const CnMatchResult m = matchCn(sd, td, skp, tkp, p);
    const double match_ms = tm.ms();
    std::vector<Eigen::Vector3d> ps, pt;
    std::size_t correct = 0;
    for (const auto& [i, j] : m.pairs) {
      ps.push_back(skp[i]);
      pt.push_back(tkp[j]);
      const Eigen::Vector3d g = gt.apply(skp[i]);
      correct += std::hypot(g.x() - tkp[j].x(), g.y() - tkp[j].y()) < gt_match;
    }
    for (const char* solver : {"svd", "svd+ransac"}) {
      Timer ts2;
      std::optional<RigidTransform> T;
      std::size_t inl = 0;
      if (std::string(solver) == "svd") {
        T = kabsch(ps, pt);
      } else {
        const auto rr = kabschRansac(ps, pt, rp);
        T = rr.T;
        inl = rr.inliers;
      }
      const double ms = match_ms + ts2.ms();
      RegistrationError e;
      e.rot_deg = e.trans_m = e.e_p = std::nan("");
      if (T) e = evaluate(*T, gt, cloud.points);
      const RigidTransform Tv = T ? *T : RigidTransform{};
      o << job.label << "," << thr << "," << (std::abs(thr - 0.05) < 1e-12 ? "paper" : "sensitivity(not paper)") << ","
        << solver << "," << skp.size() << "," << tkp.size() << "," << ss.descriptors << "," << ts.descriptors << ","
        << ss.enc2_sector0_is_p1 + ts.enc2_sector0_is_p1 << ","
        << ss.enc3_sector0_is_p1_or_p2 + ts.enc3_sector0_is_p1_or_p2 << "," << m.gate_passed << ","
        << m.candidates << "," << m.best_score << "," << m.best_dc << "," << m.best_fm << "," << m.used_triangle
        << "," << m.triangle_set_src << "," << m.triangle_set_tgt << "," << m.triangle_matches << ","
        << m.pairs.size() << "," << correct << "," << inl << "," << (T ? 1 : 0) << "," << e.rot_deg << ","
        << e.trans_m << "," << e.e_p << "," << (T && e.success ? 1 : 0) << "," << ms;
      for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c) o << "," << Tv.R(r, c);
      for (int r = 0; r < 3; ++r) o << "," << Tv.t(r);
      o << "\n";
    }
  }
  return o.str();
}

}  // namespace

int main(int argc, char** argv) {
  try {
    auto a = app::parseArgs(argc, argv,
                            {"src_trees", "tgt_trees", "src_cloud", "gt", "thresholds", "label", "out", "N", "w",
                             "th_score", "min_dc", "merge_radius", "ransac_iters", "ransac_inlier", "ransac_seed",
                             "gt_match", "batch", "help"});
    for (const char* k : {"src_trees", "tgt_trees", "src_cloud", "gt"})
      if (!a.opts.count("batch") && !a.opts.count(k)) {
        std::cout << "usage: cm_register --src_trees=CSV --tgt_trees=CSV --src_cloud=PLY --gt=TFM [options]\n";
        return a.opts.count("help") ? 0 : 2;
      }
    auto num = [&](const char* k, double d) { return a.opts.count(k) ? std::stod(a.opts[k]) : d; };
    CnParams base;
    base.N = static_cast<int>(num("N", base.N));
    base.w = num("w", base.w);
    base.th_score = num("th_score", base.th_score);
    base.min_dc = static_cast<int>(num("min_dc", base.min_dc));
    const double merge = num("merge_radius", 0.0);
    RansacParams rp;
    rp.iterations = static_cast<int>(num("ransac_iters", rp.iterations));
    rp.inlier_dist = num("ransac_inlier", rp.inlier_dist);
    rp.seed = static_cast<std::uint64_t>(num("ransac_seed", 1));
    const double gt_match = num("gt_match", 0.3);
    const auto thresholds = parseList(a.opts.count("thresholds") ? a.opts["thresholds"] : "0.05");
    const std::string label = a.opts.count("label") ? a.opts["label"] : "";

    Settings st;
    st.base = base;
    st.merge = merge;
    st.gt_match = gt_match;
    st.rp = rp;
    st.thresholds = thresholds;
    std::vector<Job> jobs;
    if (a.opts.count("batch")) {
      std::ifstream in(a.opts["batch"]);
      if (!in) throw std::runtime_error("cannot open " + a.opts["batch"]);
      std::string line;
      while (std::getline(in, line)) {
        std::istringstream ls(line);
        Job j;
        if (!(ls >> j.src_trees >> j.tgt_trees >> j.src_cloud >> j.gt >> j.out)) continue;
        ls >> j.label;
        jobs.push_back(j);
      }
    } else {
      jobs.push_back({a.opts["src_trees"], a.opts["tgt_trees"], a.opts["src_cloud"], a.opts["gt"],
                      a.opts.count("out") ? a.opts["out"] : "", label});
    }
    std::string cloud_path;
    RawCloud cloud;
    for (const Job& job : jobs) {
      if (job.src_cloud != cloud_path) {
        cloud = readPointCloud(job.src_cloud);
        cloud_path = job.src_cloud;
      }
      const std::string csv = runJob(job, st, cloud);
      if (!job.out.empty()) {
        std::ofstream f(job.out);
        if (!f) throw std::runtime_error("cannot write " + job.out);
        f << csv;
      }
      if (!a.opts.count("batch")) std::cout << csv;
    }
    if (a.opts.count("batch")) std::cout << "ran " << jobs.size() << " jobs\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
