#include "cm/config.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cm {

namespace {
std::string trim(const std::string& s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  const auto e = s.find_last_not_of(" \t\r\n");
  return s.substr(b, e - b + 1);
}

double parseDouble(const std::string& key, const std::string& v) {
  std::size_t pos = 0;
  double x;
  try {
    x = std::stod(v, &pos);
  } catch (...) {
    throw std::runtime_error("config: " + key + ": not a number: '" + v + "'");
  }
  if (pos != v.size()) throw std::runtime_error("config: " + key + ": trailing characters in '" + v + "'");
  return x;
}

long long parseInt(const std::string& key, const std::string& v) {
  std::size_t pos = 0;
  long long x;
  try {
    x = std::stoll(v, &pos);
  } catch (...) {
    throw std::runtime_error("config: " + key + ": not an integer: '" + v + "'");
  }
  if (pos != v.size()) throw std::runtime_error("config: " + key + ": trailing characters in '" + v + "'");
  return x;
}
}  // namespace

Config::Config() { buildRegistry(); }
Config::Config(const Config& o)
    : scanner(o.scanner), dtm(o.dtm), height(o.height), voxel(o.voxel), normals(o.normals),
      retention(o.retention), budget(o.budget), cluster(o.cluster), cylinder(o.cylinder),
      intersect(o.intersect), experiment(o.experiment) {
  buildRegistry();  // pointers must refer to this object, not o
}
Config& Config::operator=(const Config& o) {
  if (this != &o) {
    scanner = o.scanner; dtm = o.dtm; height = o.height; voxel = o.voxel; normals = o.normals;
    retention = o.retention; budget = o.budget; cluster = o.cluster; cylinder = o.cylinder;
    intersect = o.intersect; experiment = o.experiment;
  }
  return *this;
}

void Config::buildRegistry() {
  registry_.clear();
  auto add = [&](const std::string& k, Ptr p, bool unspec, const std::string& note) {
    registry_[k] = Entry{p, unspec, note};
  };
  const bool U = true, P = false;  // U: 論文未指定, P: paper value / our fixed design
  add("scanner.x0", &scanner.x0, U, "scanner x in original coordinates (default assumes origin)");
  add("scanner.y0", &scanner.y0, U, "scanner y");
  add("scanner.z0", &scanner.z0, U, "scanner z");
  add("scanner.use_3d_distance", &scanner.use_3d_distance, P, "d = 3-D instead of horizontal distance");
  add("scanner.nearest_warn", &scanner.nearest_warn, U, "warn if nearest point farther than this [m]");
  add("scanner.max_local_coord", &scanner.max_local_coord, U, "error if |local coord| exceeds this [m]");

  add("dtm.cell", &dtm.cell, U, "DTM grid cell size [m]");
  add("dtm.ground_select", &dtm.ground_select, U,
      "IMPLEMENTATION CHOICE: supported_lowest (default; lowest point with support_count points within support_dz "
      "above) | percentile (k-th percentile, kept for comparison)");
  add("dtm.support_count", &dtm.support_count, U, "supported_lowest: required points above the candidate");
  add("dtm.support_dz", &dtm.support_dz, U, "supported_lowest: height window above the candidate [m]");
  add("dtm.percentile", &dtm.percentile, U, "k-th percentile of z per cell");
  add("dtm.min_points", &dtm.min_points, U, "min points per cell for a ground sample");
  add("dtm.fit_radius", &dtm.fit_radius, U, "plane-fit window half width [cells]");
  add("dtm.fill_max_radius", &dtm.fill_max_radius, U, "max window half width when filling [cells]");
  add("dtm.outlier_above", &dtm.outlier_above, U, "reject ground samples above neighbour plane + this [m]");

  add("height.zmin", &height.zmin, P, "paper: 0 m");
  add("height.zmax", &height.zmax, P, "paper: 3 m");
  add("height.neg_tol", &height.neg_tol, U, "negative tolerance below zmin [m]");

  add("voxel.leaf", &voxel.leaf, P, "paper: 1 cm, keep original point nearest to voxel centroid");

  add("normals.radius", &normals.radius, P, "paper: 10 cm");
  add("normals.octree_res", &normals.octree_res, U, "octree resolution [m]");
  add("normals.min_neighbors", &normals.min_neighbors, U, "min neighbours for PCA");
  add("normals.vert_threshold", &normals.vert_threshold, P, "paper: verticality > 0.9 (0.8 = optional, NOT the paper setting)");

  add("retention.version", &retention.version, P, "step | linear_a | linear_mid | physical");
  add("retention.linear_slope", &retention.linear_slope, P, "our modification: 0.05 / m");
  add("retention.physical_d0", &retention.physical_d0, P, "our modification: d0 [m]");
  add("retention.physical_pmin", &retention.physical_pmin, P, "our modification: P_min");
  add("retention.hash_quantum", &retention.hash_quantum, P, "coordinate quantum for per-point hash [m]");

  add("budget.rel_tol", &budget.rel_tol, P, "budget match relative tolerance");
  add("budget.abs_tol", &budget.abs_tol, P, "budget match absolute tolerance [points]");
  add("budget.max_iter", &budget.max_iter, P, "bisection iterations");
  add("budget.intercept_lo", &budget.intercept_lo, P, "linear intercept search range");
  add("budget.intercept_hi", &budget.intercept_hi, P, "");
  add("budget.d0_lo", &budget.d0_lo, P, "physical d0 search range [m]");
  add("budget.d0_hi", &budget.d0_hi, P, "");

  add("cluster.tolerance", &cluster.tolerance, U, "Euclidean clustering distance [m]");
  add("cluster.min_points", &cluster.min_points, U, "min cluster size");
  add("cluster.max_points", &cluster.max_points, U, "max cluster size");

  add("cylinder.dist_threshold", &cylinder.dist_threshold, U, "RANSAC inlier distance [m]");
  add("cylinder.max_iterations", &cylinder.max_iterations, U, "RANSAC max iterations");
  add("cylinder.probability", &cylinder.probability, U, "RANSAC success probability (adaptive stop)");
  add("cylinder.radius_min", &cylinder.radius_min, U, "post-fit radius check [m]");
  add("cylinder.radius_max", &cylinder.radius_max, U, "post-fit radius check [m]");
  add("cylinder.normal_weight", &cylinder.normal_weight, U, "RANSAC normal distance weight");
  add("cylinder.max_tilt_deg", &cylinder.max_tilt_deg, U, "post-fit axis tilt check [deg]");
  add("cylinder.min_inlier_ratio", &cylinder.min_inlier_ratio, U,
      "post-fit inlier ratio check; IMPLEMENTATION CHOICE: inliers counted by geometric point-to-surface distance");
  add("cylinder.check_normal_consistency", &cylinder.check_normal_consistency, U,
      "OUR ADDITION (not in paper): enable normal-consistency check");
  add("cylinder.check_arc_coverage", &cylinder.check_arc_coverage, U,
      "OUR ADDITION (not in paper): enable arc-coverage check");
  add("cylinder.normal_max_angle_deg", &cylinder.normal_max_angle_deg, U,
      "normal consistency: max angle between point normal and fitted surface normal [deg]");
  add("cylinder.min_normal_ratio", &cylinder.min_normal_ratio, U,
      "normal consistency: min fraction of points within normal_max_angle_deg");
  add("cylinder.min_arc_deg", &cylinder.min_arc_deg, U,
      "min angular coverage of geometric inliers around the axis [deg] (5-degree bins)");
  add("cylinder.min_points", &cylinder.min_points, U, "clusters smaller than this are not fitted");
  add("cylinder.lm_refit", &cylinder.lm_refit, U, "Levenberg-Marquardt refit (task spec, not in paper)");
  add("cylinder.lm_passes", &cylinder.lm_passes, U, "IMPLEMENTATION CHOICE: LM refit passes on geometric inliers");
  add("cylinder.seed_mode", &cylinder.seed_mode, P, "follow | fixed");
  add("cylinder.fixed_seed", &cylinder.fixed_seed, P, "used when seed_mode = fixed");

  add("intersect.tol", &intersect.tol, U, "axis/DTM intersection tolerance [m]");
  add("intersect.max_iter", &intersect.max_iter, U, "");
  add("intersect.search_range", &intersect.search_range, U, "bracket half-range along axis [m]");
  add("intersect.scan_step", &intersect.scan_step, U, "bracket scan step [m]");

  add("experiment.n_seeds", &experiment.n_seeds, P, "seeds per version");
  add("experiment.seed_base", &experiment.seed_base, P, "first seed");
  add("experiment.profiles", &experiment.profiles, P,
      "cm_compare: both | baseline (extra checks off) | improved (extra checks on)");
  add("experiment.match_radius", &experiment.match_radius, P, "reference matching radius [m]");
}

void Config::set(const std::string& key, const std::string& value_in) {
  const auto it = registry_.find(key);
  if (it == registry_.end()) throw std::runtime_error("config: unknown key '" + key + "'");
  const std::string v = trim(value_in);
  std::visit(
      [&](auto* p) {
        using T = std::remove_pointer_t<decltype(p)>;
        if constexpr (std::is_same_v<T, double>) {
          *p = parseDouble(key, v);
        } else if constexpr (std::is_same_v<T, int>) {
          *p = static_cast<int>(parseInt(key, v));
        } else if constexpr (std::is_same_v<T, std::uint64_t>) {
          const long long x = parseInt(key, v);
          if (x < 0) throw std::runtime_error("config: " + key + " must be >= 0");
          *p = static_cast<std::uint64_t>(x);
        } else if constexpr (std::is_same_v<T, bool>) {
          if (v == "1" || v == "true" || v == "yes") *p = true;
          else if (v == "0" || v == "false" || v == "no") *p = false;
          else throw std::runtime_error("config: " + key + ": not a bool: '" + v + "'");
        } else {
          *p = v;
        }
      },
      it->second.ptr);
  if (key == "scanner.x0" || key == "scanner.y0" || key == "scanner.z0") scanner.position_given = true;
}

void Config::loadIni(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("config: cannot open " + path);
  std::string line, section;
  int lineno = 0;
  while (std::getline(in, line)) {
    ++lineno;
    const auto c = line.find_first_of("#;");
    if (c != std::string::npos) line = line.substr(0, c);
    line = trim(line);
    if (line.empty()) continue;
    if (line.front() == '[') {
      if (line.back() != ']') throw std::runtime_error(path + ":" + std::to_string(lineno) + ": bad section");
      section = trim(line.substr(1, line.size() - 2));
      continue;
    }
    const auto eq = line.find('=');
    if (eq == std::string::npos) throw std::runtime_error(path + ":" + std::to_string(lineno) + ": expected key = value");
    const std::string k = trim(line.substr(0, eq));
    set(section.empty() ? k : section + "." + k, line.substr(eq + 1));
  }
}

std::vector<std::string> Config::applyArgs(const std::vector<std::string>& args) {
  std::vector<std::string> rest;
  for (const auto& a : args) {
    if (a.rfind("--", 0) == 0) {
      const auto eq = a.find('=');
      const std::string k = a.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
      if (registry_.count(k)) {
        if (eq == std::string::npos) throw std::runtime_error("missing value for --" + k);
        set(k, a.substr(eq + 1));
        continue;
      }
    }
    rest.push_back(a);
  }
  return rest;
}

void Config::dump(std::ostream& os) const {
  os << "# effective configuration  ([UNSPECIFIED] = 論文未指定, chosen by us)\n";
  std::string section;
  for (const auto& [k, e] : registry_) {
    const auto dot = k.find('.');
    const std::string s = k.substr(0, dot);
    if (s != section) {
      os << "\n[" << s << "]\n";
      section = s;
    }
    // An unset scanner position is written commented out: loading it back must
    // not mark the position as explicitly given (that would bypass the
    // large-coordinate stop in prepareCloud).
    const bool unset_scanner = !scanner.position_given && (k == "scanner.x0" || k == "scanner.y0" || k == "scanner.z0");
    if (unset_scanner) os << "# ";
    os << k.substr(dot + 1) << " = ";
    std::visit(
        [&](auto* p) {
          using T = std::remove_pointer_t<decltype(p)>;
          if constexpr (std::is_same_v<T, bool>) os << (*p ? "true" : "false");
          else if constexpr (std::is_same_v<T, double>) {
            std::ostringstream ss;
            ss.precision(10);
            ss << *p;
            os << ss.str();
          } else os << *p;
        },
        e.ptr);
    os << "    # " << (e.unspecified ? "[UNSPECIFIED] " : "") << e.note << "\n";
  }
  os << "# scanner.position_given = " << (scanner.position_given ? "true" : "false") << "\n";
}

}  // namespace cm
