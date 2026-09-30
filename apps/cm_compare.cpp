// Comparison experiment: 4 versions x N seeds, raw and budget-matched tables.
//   cm_compare --input=scan.las --out=outdir [--ref=trees.csv] [--config=INI]
//              [--experiment.n_seeds=10] [--experiment.profiles=both] [--section.key=value ...] [--quiet]
// Runs the baseline profile (extra checks off) and the improved profile
// (extra checks on) and reports them in separate tables.
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "app_common.hpp"
#include "cm/experiment.hpp"

using namespace cm;

int main(int argc, char** argv) {
  try {
    auto a = app::parseArgs(argc, argv, {"input", "out", "ref", "quiet", "help"});
    if (a.opts.count("help") || !a.opts.count("input") || !a.opts.count("out")) {
      std::cout << "usage: cm_compare --input=FILE --out=DIR [--ref=REF.csv] [--config=INI] [--quiet]\n"
                   "                  [--section.key=value ...]\n";
      return a.opts.count("help") ? 0 : 2;
    }
    const Config& cfg = a.cfg;
    const std::string out = a.opts["out"];
    app::ensureDir(out);
    app::writeConfigDump(cfg, out + "/effective_config.ini");

    const RawCloud raw = readPointCloud(a.opts["input"]);
    const LoadedCloud lc = prepareCloud(raw, cfg.scanner);
    std::cout << formatReport(lc.report, raw) << "\n";
    const Preprocessed pre = preprocess(lc, cfg);

    std::vector<Eigen::Vector2d> refs;
    if (a.opts.count("ref"))
      for (const auto& q : readReferencePositions(a.opts["ref"]))
        refs.emplace_back(q.x() - pre.offset.x(), q.y() - pre.offset.y());

    // Two profiles, reported separately. Stages 1-4 are shared (the extra
    // checks only act in stage 7).
    //   baseline: extra checks (normal consistency, arc coverage) OFF
    //   improved: extra checks ON
    // Both use the same implementation choices (DTM method, geometric inlier
    // counting, LM refit on geometric inliers); neither is "the paper's method".
    struct Profile {
      std::string name, title;
      bool checks;
    };
    std::vector<Profile> profiles;
    const std::string sel = cfg.experiment.profiles;
    if (sel != "both" && sel != "baseline" && sel != "improved")
      throw std::runtime_error("experiment.profiles must be both, baseline or improved");
    if (sel != "improved")
      profiles.push_back({"baseline", "未啟用額外檢查的基準設定 (baseline: normal-consistency and arc-coverage checks OFF)", false});
    if (sel != "baseline")
      profiles.push_back({"improved", "啟用檢查的改良設定 (improved: normal-consistency and arc-coverage checks ON)", true});

    std::ofstream md(out + "/summary.md");
    const bool quiet = a.opts.count("quiet");
    auto both_out = [&](const std::string& text) {
      std::cout << text;
      md << text;
    };
    {
      std::ostringstream h;
      h << "> Synthetic/experimental output. Implementation choices (not specified by the paper) are marked in "
           "effective_config.ini as [UNSPECIFIED].\n\n"
        << "Shared stages 1-4 (computed once, identical for all profiles/versions/seeds):\n\n"
        << "| stage | points | time [ms] |\n|---|---|---|\n";
      for (const auto& s : pre.stages) h << "| " << s.name << " | " << s.points << " | " << s.ms << " |\n";
      both_out(h.str());
    }
    for (const auto& pf : profiles) {
      Config pc = cfg;
      pc.cylinder.check_normal_consistency = pf.checks;
      pc.cylinder.check_arc_coverage = pf.checks;
      const std::string pout = out + "/" + pf.name;
      app::ensureDir(pout);
      app::writeConfigDump(pc, pout + "/effective_config.ini");
      std::ofstream log(pout + "/runs.log");
      app::TeeBuf tee(log.rdbuf(), quiet ? nullptr : std::cout.rdbuf());
      std::ostream logs(&tee);
      logs << "#### profile: " << pf.title << "\n";
      const auto res = runExperiment(pre, pc, refs, pout, &logs);
      logs.flush();
      std::ostringstream t;
      t << "\n## " << pf.title << "\n\n";
      writeSummaryMarkdown(t, pf.name + " / Raw (P(d) as defined)", res.raw, !refs.empty());
      writeSummaryMarkdown(t,
                           pf.name + " / Budget-matched (kept count = step's per seed; linear_a and linear_mid "
                                     "coincide as linear*)",
                           res.matched, !refs.empty());
      both_out(t.str());
      writeSummaryCsv(pout + "/summary_raw.csv", res.raw, !refs.empty());
      writeSummaryCsv(pout + "/summary_matched.csv", res.matched, !refs.empty());
    }
    std::cout << "wrote summary.md and, per profile (";
    for (const auto& pf : profiles) std::cout << " " << pf.name << "/";
    std::cout << " ): per-run CSVs, summary_raw.csv, summary_matched.csv, runs.log, effective_config.ini under "
              << out << "\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
