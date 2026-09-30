// End-to-end on the synthetic scene: sloped ground (~12.6 deg) + 5 stems,
// one tilted 10 deg uphill and one 15 deg downhill, density ~ 1/d^2, 3 mm noise.
#include <gtest/gtest.h>

#include <cmath>

#include "cm/matching.hpp"
#include "cm/pipeline.hpp"
#include "cm/synth.hpp"

using namespace cm;

namespace {
struct Scene {
  std::vector<SynthTruth> truth;
  Preprocessed pre;
  Config cfg;
};
const Scene& scene() {
  static const Scene s = [] {
    Scene sc;
    const RawCloud raw = makeSynthetic(SynthParams::defaultScene(), &sc.truth);
    sc.pre = preprocess(prepareCloud(raw, sc.cfg.scanner), sc.cfg);
    return sc;
  }();
  return s;
}
double plane(double x, double y) { return -1.5 + 0.2 * x + 0.1 * y; }
}  // namespace

TEST(Synthetic, DtmMatchesSlopedGround) {
  const auto& s = scene();
  double maxerr = 0;
  int n = 0;
  for (double x = -12; x <= 12; x += 0.5)
    for (double y = -12; y <= 12; y += 0.5) {
      const auto h = s.pre.dtm.height(x, y);
      if (!h) continue;
      maxerr = std::max(maxerr, std::abs(*h - plane(x, y)));
      ++n;
    }
  EXPECT_GT(n, 2000);
  EXPECT_LT(maxerr, 0.02);
}

TEST(Synthetic, VerticalityRemovesGround) {
  const auto& s = scene();
  // Vertical candidates must be near a stem surface, not on the ground plane.
  std::size_t on_ground = 0;
  for (int i : s.pre.vertical) {
    const auto& p = (*s.pre.voxel)[i];
    if (std::abs(p.z - plane(p.x, p.y)) < 0.02) {
      bool near_stem = false;
      for (const auto& t : s.truth) near_stem |= std::hypot(p.x - t.position.x(), p.y - t.position.y()) < t.radius + 0.1;
      on_ground += !near_stem;
    }
  }
  EXPECT_LT(double(on_ground), 0.01 * s.pre.vertical.size());
}

class SyntheticVersions : public ::testing::TestWithParam<RetentionKind> {};

TEST_P(SyntheticVersions, RecoversAllStems) {
  const auto& s = scene();
  const auto m = RetentionModel::make(GetParam(), s.cfg.retention);
  for (std::uint64_t seed : {1, 2, 3}) {
    const RunResult r = runStages(s.pre, m, seed, s.cfg);
    std::vector<Eigen::Vector2d> det, ref;
    for (const auto& t : r.trees) det.emplace_back(t.position.x(), t.position.y());
    for (const auto& t : s.truth) ref.emplace_back(t.position.x(), t.position.y());
    const auto mt = matchPositions(det, ref, 0.5);
    ASSERT_EQ(mt.pairs.size(), s.truth.size()) << toString(GetParam()) << " seed " << seed;
    EXPECT_EQ(r.trees.size(), s.truth.size()) << "false positives";
    for (std::size_t k = 0; k < mt.pairs.size(); ++k) {
      const auto& tr = r.trees[mt.pairs[k].first];
      const auto& gt = s.truth[mt.pairs[k].second];
      EXPECT_LT(mt.errors[k], 0.02) << "stem " << mt.pairs[k].second << " tilt " << gt.tilt_deg;
      EXPECT_LT(std::abs(tr.radius - gt.radius), 0.01) << "stem " << mt.pairs[k].second;
      EXPECT_LT(std::abs(tr.position.z() - gt.position.z()), 0.02) << "stem " << mt.pairs[k].second;
      EXPECT_NEAR(tr.tilt_deg, gt.tilt_deg, 2.0) << "stem " << mt.pairs[k].second;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(AllVersions, SyntheticVersions,
                         ::testing::Values(RetentionKind::Step, RetentionKind::LinearA, RetentionKind::LinearMid,
                                           RetentionKind::Physical),
                         [](const auto& info) { return toString(info.param); });

TEST(Synthetic, SurveyCoordinatesWithScannerPosition) {
  SynthParams p = SynthParams::defaultScene();
  p.offset = Eigen::Vector3d(512345.678, 4412345.678, 123.4);
  std::vector<SynthTruth> truth;
  const RawCloud raw = makeSynthetic(p, &truth);
  Config cfg;
  EXPECT_THROW(prepareCloud(raw, cfg.scanner), std::runtime_error);  // default origin: must stop
  cfg.set("scanner.x0", "512345.678");
  cfg.set("scanner.y0", "4412345.678");
  cfg.set("scanner.z0", "123.4");
  const Preprocessed pre = preprocess(prepareCloud(raw, cfg.scanner), cfg);
  const RunResult r = runStages(pre, RetentionModel::make(RetentionKind::Step, cfg.retention), 1, cfg);
  std::vector<Eigen::Vector2d> det, ref;
  for (const auto& t : r.trees) det.emplace_back((t.position + pre.offset).head<2>());
  for (const auto& t : truth) ref.emplace_back(t.position.head<2>());
  const auto mt = matchPositions(det, ref, 0.5);
  EXPECT_EQ(mt.pairs.size(), truth.size());
  EXPECT_LT(mt.rmse(), 0.02);
}
