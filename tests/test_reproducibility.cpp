#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "cm/experiment.hpp"
#include "cm/synth.hpp"

using namespace cm;
namespace fs = std::filesystem;

namespace {
std::string slurp(const std::string& p) {
  std::ifstream in(p);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}
std::string runToCsv(const RawCloud& raw, const Config& cfg, RetentionKind k, std::uint64_t seed, const std::string& tag) {
  const Preprocessed pre = preprocess(prepareCloud(raw, cfg.scanner), cfg);  // full pipeline each time
  const RunResult r = runStages(pre, RetentionModel::make(k, cfg.retention), seed, cfg);
  const auto dir = fs::temp_directory_path() / "cm_test_repro";
  fs::create_directories(dir);
  const std::string a = (dir / (tag + "_trees.csv")).string(), b = (dir / (tag + "_clusters.csv")).string();
  writeTreesCsv(a, r, pre.offset);
  writeClustersCsv(b, r, pre.offset);
  return slurp(a) + "\n---\n" + slurp(b);
}
}  // namespace

TEST(Reproducibility, SameSeedIdenticalOutput) {
  const RawCloud raw = makeSynthetic(SynthParams::defaultScene(), nullptr);
  Config cfg;
  for (auto k : {RetentionKind::Step, RetentionKind::Physical}) {
    const auto a = runToCsv(raw, cfg, k, 7, "a");
    const auto b = runToCsv(raw, cfg, k, 7, "b");
    EXPECT_EQ(a, b) << toString(k);
    EXPECT_GT(a.size(), 100u);
  }
}

TEST(Reproducibility, InputOrderDoesNotChangeKeptSet) {
  RawCloud raw = makeSynthetic(SynthParams::defaultScene(), nullptr);
  Config cfg;
  auto keptPoints = [&](const RawCloud& rc) {
    const Preprocessed pre = preprocess(prepareCloud(rc, cfg.scanner), cfg);
    std::vector<char> flags;
    runStages(pre, RetentionModel::make(RetentionKind::Step, cfg.retention), 3, cfg, nullptr, &flags);
    std::vector<std::tuple<float, float, float>> v;
    for (std::size_t k = 0; k < flags.size(); ++k)
      if (flags[k]) {
        const auto& p = (*pre.voxel)[pre.vertical[k]];
        v.emplace_back(p.x, p.y, p.z);
      }
    std::sort(v.begin(), v.end());
    return v;
  };
  const auto a = keptPoints(raw);
  std::reverse(raw.points.begin(), raw.points.end());
  const auto b = keptPoints(raw);
  // Voxel representatives are chosen by distance to the centroid (ties -> lower
  // index), so reversing the input may only change exact ties; require equality.
  EXPECT_EQ(a, b);
}

TEST(Reproducibility, CommonRandomNumbersSubsetInPipeline) {
  const RawCloud raw = makeSynthetic(SynthParams::defaultScene(), nullptr);
  Config cfg;
  const Preprocessed pre = preprocess(prepareCloud(raw, cfg.scanner), cfg);
  for (std::uint64_t seed : {1, 2}) {
    const auto u = computeUniforms(pre, seed, cfg);
    std::vector<char> fs_, fa, fm;
    runStages(pre, RetentionModel::make(RetentionKind::Step, cfg.retention), seed, cfg, &u, &fs_);
    runStages(pre, RetentionModel::make(RetentionKind::LinearA, cfg.retention), seed, cfg, &u, &fa);
    runStages(pre, RetentionModel::make(RetentionKind::LinearMid, cfg.retention), seed, cfg, &u, &fm);
    for (std::size_t k = 0; k < fs_.size(); ++k) {
      ASSERT_LE(fs_[k], fa[k]) << "step kept a point linear_a dropped";
      ASSERT_LE(fm[k], fa[k]) << "linear_mid kept a point linear_a dropped";
    }
  }
}
