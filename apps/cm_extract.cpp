// Single run: one P(d) version, one seed.
//   cm_extract --input=scan.las --out=outdir [--version=step] [--seed=1]
//              [--config=config/default.ini] [--scanner.x0=... --section.key=value ...]
#include <iomanip>
#include <iostream>

#include "app_common.hpp"
#include "cm/experiment.hpp"
#include "cm/pipeline.hpp"

using namespace cm;

int main(int argc, char** argv) {
  try {
    auto a = app::parseArgs(argc, argv, {"input", "out", "version", "seed", "help"});
    if (a.opts.count("help") || !a.opts.count("input")) {
      std::cout << "usage: cm_extract --input=FILE [--out=DIR] [--version=step|linear_a|linear_mid|physical]\n"
                   "                  [--seed=N] [--config=INI] [--section.key=value ...]\n";
      return a.opts.count("help") ? 0 : 2;
    }
    Config& cfg = a.cfg;
    if (a.opts.count("version")) cfg.retention.version = a.opts["version"];
    const std::uint64_t seed = a.opts.count("seed") ? std::stoull(a.opts["seed"]) : cfg.experiment.seed_base;
    const std::string out = a.opts.count("out") ? a.opts["out"] : "";
    const RetentionModel model = RetentionModel::make(parseRetentionKind(cfg.retention.version), cfg.retention);

    const RawCloud raw = readPointCloud(a.opts["input"]);
    const LoadedCloud lc = prepareCloud(raw, cfg.scanner);
    std::cout << formatReport(lc.report, raw) << "\n";
    const Preprocessed pre = preprocess(lc, cfg);
    std::cout << "DTM: " << pre.dtm.stats().nx << "x" << pre.dtm.stats().ny << " cells, "
              << pre.dtm.stats().cells_with_sample << " with ground sample, " << pre.dtm.stats().samples_rejected
              << " rejected, " << pre.dtm.stats().cells_filled << " filled; points outside DTM: "
              << pre.height_no_dtm << "; normals invalid (< min_neighbors): " << pre.normals_invalid << "\n";

    std::vector<char> flags;
    const auto u = computeUniforms(pre, seed, cfg);
    const RunResult r = runStages(pre, model, seed, cfg, &u, &flags);
    printRun(std::cout, r, pre);

    std::cout << "retention check (empirical keep rate vs mean P(d) per 1 m bin):\n"
              << "   d[m]        n     kept   empirical   mean P\n";
    for (const auto& b : retentionBins(pre.dist, flags, model)) {
      if (!b.n) continue;
      std::cout << std::setw(4) << int(b.d_lo) << "-" << std::left << std::setw(4) << int(b.d_hi) << std::right
                << std::setw(8) << b.n << std::setw(9) << b.kept << std::setw(12) << std::setprecision(3)
                << double(b.kept) / double(b.n) << std::setw(9) << b.p_mean << "\n";
    }
    if (!out.empty()) {
      app::ensureDir(out);
      const std::string stem = out + "/" + r.label + "_seed" + std::to_string(seed);
      writeTreesCsv(stem + "_trees.csv", r, pre.offset);
      writeClustersCsv(stem + "_clusters.csv", r, pre.offset);
      writeRetentionBinsCsv(stem + "_retention_bins.csv", retentionBins(pre.dist, flags, model));
      app::writeConfigDump(cfg, out + "/effective_config.ini");
      std::cout << "wrote " << stem << "_{trees,clusters,retention_bins}.csv and effective_config.ini\n";
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
