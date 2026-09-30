// Comparison experiment: 4 P(d) versions x N seeds on one preprocessed input,
// plus the budget-matched variants (linear*, physical*; target = step's kept
// count for the same seed). Writes per-run CSVs and summary tables.
#pragma once
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "cm/budget.hpp"
#include "cm/matching.hpp"
#include "cm/pipeline.hpp"

namespace cm {

// CSV of detected trees in ORIGINAL coordinates (local + offset).
void writeTreesCsv(const std::string& path, const RunResult& r, const Eigen::Vector3d& offset);
// CSV of every cluster sent to RANSAC with its fit diagnostics.
void writeClustersCsv(const std::string& path, const RunResult& r, const Eigen::Vector3d& offset);
void writeRetentionBinsCsv(const std::string& path, const std::vector<RetentionBin>& bins);
// Human-readable per-run log: stage counts/times, trees, success rate, failure reasons.
void printRun(std::ostream& os, const RunResult& r, const Preprocessed& pre);

struct RunMetrics {
  std::string label;
  std::uint64_t seed = 0;
  double kept = 0, trees = 0, clusters = 0, success_rate = 0;
  std::map<std::string, double> stage_ms;  // stages 5-8 + "total 5-8"
  std::optional<MatchResult> match;         // if references given
  std::optional<BudgetResult> budget;       // for matched runs
  std::array<std::size_t, int(FitFail::Count)> fail_counts{};
};

struct ExperimentOutput {
  std::vector<RunMetrics> raw, matched;
};

// refs: reference positions in the LOCAL frame (xy), empty if none.
// out_dir: where per-run CSVs go ("" = none).
ExperimentOutput runExperiment(const Preprocessed& pre, const Config& cfg, const std::vector<Eigen::Vector2d>& refs,
                               const std::string& out_dir, std::ostream* log);

// Markdown table: one column per label (in first-seen order), mean ± sample std over seeds.
void writeSummaryMarkdown(std::ostream& os, const std::string& title, const std::vector<RunMetrics>& runs,
                          bool with_refs);
void writeSummaryCsv(const std::string& path, const std::vector<RunMetrics>& runs, bool with_refs);

}  // namespace cm
