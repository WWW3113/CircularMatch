// All tunable parameters. Anything the paper does not specify is registered
// with unspecified=true and printed as [論文未指定 / UNSPECIFIED] in the
// effective-config dump, so every run records which choices were ours.
#pragma once
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace cm {

struct ScannerParams {
  double x0 = 0.0, y0 = 0.0, z0 = 0.0;  // scanner position, original coordinates [m] (論文未指定: 假設為原點)
  bool position_given = false;          // true if any of x0/y0/z0 was set explicitly
  bool use_3d_distance = false;         // false: d = horizontal distance; true: 3-D distance
  double nearest_warn = 3.0;            // warn if nearest point is farther than this (horizontal) [m]
  double max_local_coord = 1.0e4;       // error if |local coordinate| exceeds this [m] (float precision)
};

struct DtmParams {
  double cell = 0.5;           // grid cell size [m]
  double percentile = 5.0;     // k-th percentile of z per cell = ground sample
  int min_points = 3;          // cells with fewer points get no ground sample
  int fit_radius = 2;          // plane-fit window half width [cells] (5x5)
  int fill_max_radius = 5;     // window may grow up to this to fill empty cells
  double outlier_above = 0.5;  // ground samples higher than neighbour plane + this are rejected [m]
};

struct HeightParams {
  double zmin = 0.0;     // paper: 0 m
  double zmax = 3.0;     // paper: 3 m
  double neg_tol = 0.05; // accept z - DTM >= zmin - neg_tol
};

struct VoxelParams {
  double leaf = 0.01;  // paper: 1 cm
};

struct NormalParams {
  double radius = 0.10;       // paper: 10 cm
  double octree_res = 0.05;   // octree leaf resolution [m]
  int min_neighbors = 5;      // fewer -> point discarded
  double vert_threshold = 0.9;  // paper: verticality > 0.9
};

struct RetentionParams {
  std::string version = "step";  // step | linear_a | linear_mid | physical
  double linear_slope = 0.05;
  double physical_d0 = 10.0;
  double physical_pmin = 0.1;
  double hash_quantum = 1e-4;  // coordinate quantisation for the per-point hash [m]
};

struct BudgetParams {
  double rel_tol = 0.001;  // |kept - target| <= max(abs_tol, rel_tol * target)
  int abs_tol = 1;
  int max_iter = 60;
  double intercept_lo = -0.5, intercept_hi = 1.0;
  double d0_lo = 0.1, d0_hi = 1000.0;
};

struct ClusterParams {
  double tolerance = 0.10;
  int min_points = 30;
  int max_points = 10000000;
};

struct CylinderParams {
  double dist_threshold = 0.02;
  int max_iterations = 1000;
  double probability = 0.99;
  double radius_min = 0.03, radius_max = 1.0;
  double normal_weight = 0.1;
  double max_tilt_deg = 20.0;
  double min_inlier_ratio = 0.5;
  int min_points = 30;
  bool lm_refit = true;
  std::string seed_mode = "follow";  // follow: derived from run seed; fixed: from fixed_seed
  std::uint64_t fixed_seed = 12345;
};

struct IntersectParams {
  double tol = 0.001;         // |f(t)| tolerance [m]
  int max_iter = 50;
  double search_range = 5.0;  // bracket search half-range along axis [m]
  double scan_step = 0.1;     // bracket scan step [m]
};

struct ExperimentParams {
  int n_seeds = 10;
  std::uint64_t seed_base = 1;  // seeds = seed_base .. seed_base + n_seeds - 1
  double match_radius = 0.5;    // reference matching: horizontal distance limit [m]
};

struct Config {
  ScannerParams scanner;
  DtmParams dtm;
  HeightParams height;
  VoxelParams voxel;
  NormalParams normals;
  RetentionParams retention;
  BudgetParams budget;
  ClusterParams cluster;
  CylinderParams cylinder;
  IntersectParams intersect;
  ExperimentParams experiment;

  Config();
  Config(const Config&);
  Config& operator=(const Config&);

  // key = "section.name". Throws std::runtime_error for unknown keys / bad values.
  void set(const std::string& key, const std::string& value);
  void loadIni(const std::string& path);
  // Accepts "--section.name=value"; returns the arguments it did not consume.
  std::vector<std::string> applyArgs(const std::vector<std::string>& args);
  void dump(std::ostream& os) const;

 private:
  using Ptr = std::variant<double*, int*, bool*, std::string*, std::uint64_t*>;
  struct Entry {
    Ptr ptr;
    bool unspecified;
    std::string note;
  };
  std::map<std::string, Entry> registry_;
  void buildRegistry();
};

}  // namespace cm
