// Comparison experiment: 4 versions x N seeds, raw and budget-matched tables.
//   cm_compare --input=scan.las --out=outdir [--ref=trees.csv] [--config=INI]
//              [--experiment.n_seeds=10] [--section.key=value ...] [--quiet]
#include <fstream>
#include <iostream>

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

    std::ofstream log(out + "/runs.log");
    const bool quiet = a.opts.count("quiet");
    app::TeeBuf tee(log.rdbuf(), quiet ? nullptr : std::cout.rdbuf());
    std::ostream both(&tee);
    const auto res = runExperiment(pre, cfg, refs, out, &both);

    std::ofstream md(out + "/summary.md");
    for (std::ostream* os : {static_cast<std::ostream*>(&std::cout), static_cast<std::ostream*>(&md)}) {
      *os << "\nShared stages 1-4 (computed once, identical for all versions/seeds):\n\n| stage | points | time [ms] |\n|---|---|---|\n";
      for (const auto& s : pre.stages) *os << "| " << s.name << " | " << s.points << " | " << s.ms << " |\n";
      *os << "\n";
      writeSummaryMarkdown(*os, "Raw (P(d) as defined)", res.raw, !refs.empty());
      writeSummaryMarkdown(*os,
                           "Budget-matched (kept count = step's per seed; linear_a and linear_mid coincide as "
                           "linear*)",
                           res.matched, !refs.empty());
    }
    writeSummaryCsv(out + "/summary_raw.csv", res.raw, !refs.empty());
    writeSummaryCsv(out + "/summary_matched.csv", res.matched, !refs.empty());
    std::cout << "wrote per-run CSVs, summary.md, summary_raw.csv, summary_matched.csv, runs.log to " << out << "\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
