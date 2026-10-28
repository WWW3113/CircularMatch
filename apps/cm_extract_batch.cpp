// Repeat one extraction configuration while sharing seed-independent stages 1-4.
// All fitting, sampling and output functions are the existing library routines.
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "app_common.hpp"
#include "cm/experiment.hpp"
#include "cm/pipeline.hpp"

using namespace cm;

int main(int argc, char** argv) {
  try {
    auto a = app::parseArgs(argc, argv, {"input", "out", "version", "help"});
    if (a.opts.count("help") || !a.opts.count("input") || !a.opts.count("out")) {
      std::cout << "usage: cm_extract_batch --input=FILE --out=DIR [--version=step]\n"
                   "  [--experiment.seed_base=1 --experiment.n_seeds=10 --config=INI --section.key=value]\n";
      return a.opts.count("help") ? 0 : 2;
    }
    Config cfg = a.cfg;
    if (a.opts.count("version")) cfg.retention.version = a.opts.at("version");
    const auto first = cfg.experiment.seed_base;
    const auto count = cfg.experiment.n_seeds;
    if (count <= 0 || std::uint64_t(count - 1) > std::numeric_limits<std::uint64_t>::max() - first)
      throw std::runtime_error("seed range must be nonempty and must not overflow uint64");
    const auto model = RetentionModel::make(parseRetentionKind(cfg.retention.version), cfg.retention);
    const auto raw = readPointCloud(a.opts.at("input"));
    const auto loaded = prepareCloud(raw, cfg.scanner);
    std::cout << formatReport(loaded.report, raw) << '\n';
    const auto pre = preprocess(loaded, cfg);
    const auto out = a.opts.at("out");
    app::ensureDir(out);
    app::writeConfigDump(cfg, out + "/effective_config.ini");
    for (int i = 0; i < count; ++i) {
      const auto seed = first + std::uint64_t(i);
      std::vector<char> flags;
      const auto uniforms = computeUniforms(pre, seed, cfg);
      const auto result = runStages(pre, model, seed, cfg, &uniforms, &flags);
      const auto stem = out + "/" + result.label + "_seed" + std::to_string(seed);
      writeTreesCsv(stem + "_trees.csv", result, pre.offset);
      writeClustersCsv(stem + "_clusters.csv", result, pre.offset);
      writeRetentionBinsCsv(stem + "_retention_bins.csv", retentionBins(pre.dist, flags, model));
      printRun(std::cout, result, pre);
      std::cout << "completed seed " << seed << std::endl;
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << '\n';
    return 1;
  }
}
