// Keypoint (stem position) extraction pipeline.
//   preprocess(): stages 1-4 (DTM + height filter, voxel, normals, verticality).
//                 Independent of P(d) and seed -> computed once and shared.
//   runStages():  stages 5-8 (retention sampling, clustering, RANSAC cylinder,
//                 axis/DTM intersection) for one P(d) model and one seed.
#pragma once
#include <array>
#include <string>
#include <vector>

#include "cm/config.hpp"
#include "cm/cylinder.hpp"
#include "cm/dtm.hpp"
#include "cm/normals.hpp"
#include "cm/retention.hpp"
#include "cm/sanity.hpp"

namespace cm {

struct StageCount {
  std::string name;
  std::size_t points = 0;
  double ms = 0;
};

struct Preprocessed {
  Eigen::Vector3d offset;   // original = local + offset
  Dtm dtm;
  Cloud::Ptr voxel;         // height-filtered, voxelised points (local frame)
  NormalCloud::Ptr normals; // aligned with voxel
  std::vector<int> vertical;   // indices into voxel with verticality > threshold
  std::vector<double> dist;    // scanner distance d of each vertical point
  std::vector<StageCount> stages;  // shared stages 1-4
  std::size_t height_no_dtm = 0;
  std::size_t normals_invalid = 0;
};

Preprocessed preprocess(const LoadedCloud& in, const Config& cfg);

struct TreeRecord {
  int cluster_id = 0;
  Eigen::Vector3d position;  // local frame
  double radius = 0, tilt_deg = 0;
  std::size_t n_points = 0, n_inliers = 0;
};

struct ClusterDiag {
  int cluster_id = 0;
  CylinderFit fit;
  bool has_position = false;
};

struct RunResult {
  std::string label;
  std::uint64_t seed = 0;
  RetentionModel model;
  std::size_t candidates = 0;  // vertical points entering stage 5
  std::size_t kept = 0;
  std::size_t clusters = 0;
  std::vector<TreeRecord> trees;
  std::vector<ClusterDiag> diag;
  std::array<std::size_t, int(FitFail::Count)> fail_counts{};
  std::vector<StageCount> stages;  // stages 5-8
  double success_rate() const { return clusters ? double(trees.size()) / double(clusters) : 0.0; }
  double total_ms() const;
};

// Per-point uniforms u for the vertical points (common random numbers).
std::vector<double> computeUniforms(const Preprocessed& pre, std::uint64_t seed, const Config& cfg);

RunResult runStages(const Preprocessed& pre, const RetentionModel& model, std::uint64_t seed, const Config& cfg,
                    const std::vector<double>* uniforms = nullptr, std::vector<char>* kept_flags = nullptr);

}  // namespace cm
