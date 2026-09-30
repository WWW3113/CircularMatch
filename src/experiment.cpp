#include "cm/experiment.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace cm {

namespace {

std::ofstream openOut(const std::string& path) {
  std::ofstream o(path);
  if (!o) throw std::runtime_error("cannot write " + path);
  o.setf(std::ios::fixed);
  return o;
}

std::string fileLabel(const std::string& label) {
  std::string s;
  for (char c : label) s += (c == '*') ? std::string("") : std::string(1, c);
  return s;
}

struct MeanStd {
  double mean = std::nan(""), std = std::nan("");
  std::size_t n = 0;
};

MeanStd meanStd(const std::vector<double>& xs) {
  MeanStd r;
  double s = 0;
  for (double x : xs)
    if (std::isfinite(x)) { s += x; ++r.n; }
  if (!r.n) return r;
  r.mean = s / double(r.n);
  if (r.n < 2) return r;
  double ss = 0;
  for (double x : xs)
    if (std::isfinite(x)) ss += (x - r.mean) * (x - r.mean);
  r.std = std::sqrt(ss / double(r.n - 1));
  return r;
}

std::string fmt(const MeanStd& m, int prec) {
  if (!m.n) return "–";
  std::ostringstream o;
  o.setf(std::ios::fixed);
  o.precision(prec);
  o << m.mean;
  if (std::isfinite(m.std)) o << " ± " << m.std;
  return o.str();
}

RunMetrics toMetrics(const RunResult& r, const std::vector<Eigen::Vector2d>& refs, double radius) {
  RunMetrics m;
  m.label = r.label;
  m.seed = r.seed;
  m.kept = double(r.kept);
  m.trees = double(r.trees.size());
  m.clusters = double(r.clusters);
  m.success_rate = r.success_rate();
  m.fail_counts = r.fail_counts;
  for (const auto& s : r.stages) m.stage_ms[s.name] = s.ms;
  m.stage_ms["total 5-8"] = r.total_ms();
  if (!refs.empty()) {
    std::vector<Eigen::Vector2d> det;
    for (const auto& t : r.trees) det.emplace_back(t.position.x(), t.position.y());
    m.match = matchPositions(det, refs, radius);
  }
  return m;
}

struct Row {
  std::string name;
  int prec;
  std::function<double(const RunMetrics&)> get;
};

std::vector<Row> rows(bool with_refs, const std::vector<RunMetrics>& runs) {
  std::vector<Row> rs = {
      {"kept points", 1, [](const RunMetrics& m) { return m.kept; }},
      {"clusters", 2, [](const RunMetrics& m) { return m.clusters; }},
      {"trees", 2, [](const RunMetrics& m) { return m.trees; }},
      {"RANSAC success rate", 3, [](const RunMetrics& m) { return m.success_rate; }},
  };
  for (int f = 1; f < int(FitFail::Count); ++f)
    rs.push_back({std::string("fail: ") + toString(FitFail(f)), 2,
                  [f](const RunMetrics& m) { return double(m.fail_counts[f]); }});
  if (!runs.empty())
    for (const auto& kv : runs.front().stage_ms) {
      const std::string k = kv.first;
      rs.push_back({"time " + k + " [ms]", 2, [k](const RunMetrics& m) {
                      const auto it = m.stage_ms.find(k);
                      return it == m.stage_ms.end() ? std::nan("") : it->second;
                    }});
    }
  bool any_budget = false;
  for (const auto& r : runs) any_budget |= r.budget.has_value();
  if (any_budget)
    rs.push_back({"budget |kept - target|", 1, [](const RunMetrics& m) {
                    return m.budget ? std::abs(double(m.budget->kept) - double(m.budget->target)) : std::nan("");
                  }});
  if (with_refs) {
    rs.push_back({"recall", 3, [](const RunMetrics& m) { return m.match ? m.match->recall() : std::nan(""); }});
    rs.push_back({"precision", 3, [](const RunMetrics& m) { return m.match ? m.match->precision() : std::nan(""); }});
    rs.push_back({"mean horiz. error [m]", 4, [](const RunMetrics& m) { return m.match ? m.match->mean_error() : std::nan(""); }});
    rs.push_back({"RMSE horiz. [m]", 4, [](const RunMetrics& m) { return m.match ? m.match->rmse() : std::nan(""); }});
  }
  return rs;
}

std::vector<std::string> labelsInOrder(const std::vector<RunMetrics>& runs) {
  std::vector<std::string> ls;
  for (const auto& r : runs)
    if (std::find(ls.begin(), ls.end(), r.label) == ls.end()) ls.push_back(r.label);
  return ls;
}

}  // namespace

void writeTreesCsv(const std::string& path, const RunResult& r, const Eigen::Vector3d& off) {
  auto o = openOut(path);
  o << "cluster_id,x,y,z,radius,tilt_deg,n_points,n_inliers\n";
  for (const auto& t : r.trees) {
    const Eigen::Vector3d p = t.position + off;
    o << t.cluster_id << "," << std::setprecision(4) << p.x() << "," << p.y() << "," << p.z() << ","
      << std::setprecision(4) << t.radius << "," << std::setprecision(3) << t.tilt_deg << "," << t.n_points << ","
      << t.n_inliers << "\n";
  }
}

void writeClustersCsv(const std::string& path, const RunResult& r, const Eigen::Vector3d& off) {
  auto o = openOut(path);
  o << "cluster_id,status,fail_mask,n_points,n_inliers,radius,tilt_deg,axis_x,axis_y,axis_z,dir_x,dir_y,dir_z,"
       "ransac_seed\n";
  for (const auto& d : r.diag) {
    const auto& f = d.fit;
    const Eigen::Vector3d ap = f.axis_point + off;
    o << d.cluster_id << "," << toString(f.fail) << "," << f.fail_mask << "," << f.n_points << "," << f.n_inliers
      << "," << std::setprecision(4) << f.radius << "," << std::setprecision(3) << f.tilt_deg << ","
      << std::setprecision(4) << ap.x() << "," << ap.y() << "," << ap.z() << "," << std::setprecision(6)
      << f.axis_dir.x() << "," << f.axis_dir.y() << "," << f.axis_dir.z() << "," << f.ransac_seed << "\n";
  }
}

void writeRetentionBinsCsv(const std::string& path, const std::vector<RetentionBin>& bins) {
  auto o = openOut(path);
  o << "d_lo,d_hi,n,kept,empirical_rate,mean_P\n";
  for (const auto& b : bins)
    o << std::setprecision(2) << b.d_lo << "," << b.d_hi << "," << b.n << "," << b.kept << "," << std::setprecision(4)
      << (b.n ? double(b.kept) / double(b.n) : 0.0) << "," << b.p_mean << "\n";
}

void printRun(std::ostream& os, const RunResult& r, const Preprocessed& pre) {
  os << "== run " << r.label << " seed " << r.seed << " : " << r.model.describe() << "\n";
  os << "  stage                points/items      time[ms]\n";
  auto line = [&](const StageCount& s, const char* tag) {
    os << "  " << std::left << std::setw(20) << s.name << std::right << std::setw(12) << s.points << std::setw(14)
       << std::fixed << std::setprecision(2) << s.ms << tag << "\n";
  };
  for (const auto& s : pre.stages) line(s, "  (shared)");
  for (const auto& s : r.stages) line(s, "");
  os << "  trees: " << r.trees.size() << " ; RANSAC success rate = " << r.trees.size() << " / " << r.clusters << " = "
     << std::setprecision(3) << r.success_rate() << "\n  failures:";
  for (int f = 1; f < int(FitFail::Count); ++f) os << " " << toString(FitFail(f)) << "=" << r.fail_counts[f];
  os << "\n";
}

ExperimentOutput runExperiment(const Preprocessed& pre, const Config& cfg, const std::vector<Eigen::Vector2d>& refs,
                               const std::string& out_dir, std::ostream* log) {
  ExperimentOutput out;
  const RetentionKind kinds[] = {RetentionKind::Step, RetentionKind::LinearA, RetentionKind::LinearMid,
                                 RetentionKind::Physical};
  const double radius = cfg.experiment.match_radius;
  for (int s = 0; s < cfg.experiment.n_seeds; ++s) {
    const std::uint64_t seed = cfg.experiment.seed_base + std::uint64_t(s);
    const auto u = computeUniforms(pre, seed, cfg);
    auto save = [&](const RunResult& r, const char* table) {
      if (log) printRun(*log, r, pre);
      if (out_dir.empty()) return;
      const std::string stem = out_dir + "/" + fileLabel(r.label) + "_" + table + "_seed" + std::to_string(seed);
      writeTreesCsv(stem + "_trees.csv", r, pre.offset);
      writeClustersCsv(stem + "_clusters.csv", r, pre.offset);
    };
    std::size_t step_kept = 0;
    RunMetrics step_metrics;
    for (auto k : kinds) {
      const RetentionModel m = RetentionModel::make(k, cfg.retention);
      std::vector<char> flags;
      const RunResult r = runStages(pre, m, seed, cfg, &u, &flags);
      save(r, "raw");
      if (!out_dir.empty() && s == 0)
        writeRetentionBinsCsv(out_dir + "/" + fileLabel(r.label) + "_raw_seed" + std::to_string(seed) +
                                  "_retention_bins.csv",
                              retentionBins(pre.dist, flags, m));
      RunMetrics mt = toMetrics(r, refs, radius);
      if (k == RetentionKind::Step) {
        step_kept = r.kept;
        step_metrics = mt;
      }
      out.raw.push_back(mt);
    }
    // budget-matched: step itself + linear* (linear_a and linear_mid coincide) + physical*
    out.matched.push_back(step_metrics);
    for (auto k : {RetentionKind::LinearA, RetentionKind::Physical}) {
      const BudgetResult b = matchBudget(RetentionModel::make(k, cfg.retention), pre.dist, u, step_kept, cfg.budget);
      if (log)
        *log << "  budget match " << b.model.describe() << " kept " << b.kept << " target " << b.target
             << (b.converged ? " (converged)" : " (NOT converged)") << "\n";
      const RunResult r = runStages(pre, b.model, seed, cfg, &u);
      save(r, "matched");
      RunMetrics mt = toMetrics(r, refs, radius);
      mt.budget = b;
      out.matched.push_back(mt);
    }
  }
  return out;
}

void writeSummaryMarkdown(std::ostream& os, const std::string& title, const std::vector<RunMetrics>& runs,
                          bool with_refs) {
  const auto labels = labelsInOrder(runs);
  os << "### " << title << "\n\n| metric |";
  for (const auto& l : labels) os << " " << l << " |";
  os << "\n|---|";
  for (std::size_t i = 0; i < labels.size(); ++i) os << "---|";
  os << "\n";
  for (const auto& row : rows(with_refs, runs)) {
    os << "| " << row.name << " |";
    for (const auto& l : labels) {
      std::vector<double> xs;
      for (const auto& r : runs)
        if (r.label == l) xs.push_back(row.get(r));
      os << " " << fmt(meanStd(xs), row.prec) << " |";
    }
    os << "\n";
  }
  std::size_t n = 0;
  for (const auto& r : runs) n += r.label == labels.front();
  os << "\nmean ± sample std over " << n << " seeds.\n\n";
}

void writeSummaryCsv(const std::string& path, const std::vector<RunMetrics>& runs, bool with_refs) {
  const auto labels = labelsInOrder(runs);
  auto o = openOut(path);
  o << "metric";
  for (const auto& l : labels) o << "," << l << "_mean," << l << "_std";
  o << "\n";
  o.precision(6);
  for (const auto& row : rows(with_refs, runs)) {
    o << row.name;
    for (const auto& l : labels) {
      std::vector<double> xs;
      for (const auto& r : runs)
        if (r.label == l) xs.push_back(row.get(r));
      const auto m = meanStd(xs);
      o << "," << m.mean << "," << m.std;
    }
    o << "\n";
  }
}

}  // namespace cm
