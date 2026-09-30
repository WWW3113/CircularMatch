// End-to-end on the synthetic scene: sloped ground (~12.6 deg) + 5 stems,
// one tilted 10 deg uphill and one 15 deg downhill, density ~ 1/d^2, 3 mm noise.
#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>

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

// Checks every detection matches a true stem (no false positives), that every
// stem with tilt <= max_required_tilt is found, and accuracy:
//  - required stems: position < 2 cm, radius < 1 cm, z < 2 cm, tilt +-2 deg
//  - optional stems (tilt above max_required_tilt, i.e. the 15-deg stem at the
//    paper's verticality threshold, which keeps only side strips): if found,
//    position < 5 cm, radius < 3 cm (10-seed runs showed up to 2.05 / 1.85 cm).
void checkRun(const RunResult& r, const std::vector<SynthTruth>& truth, double max_required_tilt,
              const std::string& ctx) {
  std::vector<Eigen::Vector2d> det, ref;
  for (const auto& t : r.trees) det.emplace_back(t.position.x(), t.position.y());
  for (const auto& t : truth) ref.emplace_back(t.position.x(), t.position.y());
  const auto mt = matchPositions(det, ref, 0.5);
  EXPECT_EQ(mt.pairs.size(), r.trees.size()) << ctx << ": false positives";
  std::vector<char> found(truth.size(), 0);
  for (std::size_t k = 0; k < mt.pairs.size(); ++k) {
    const auto& tr = r.trees[mt.pairs[k].first];
    const auto& gt = truth[mt.pairs[k].second];
    found[mt.pairs[k].second] = 1;
    const std::string id = ctx + " stem " + std::to_string(mt.pairs[k].second) + " tilt " + std::to_string(gt.tilt_deg);
    const bool required = gt.tilt_deg <= max_required_tilt;
    EXPECT_LT(mt.errors[k], required ? 0.02 : 0.05) << id;
    EXPECT_LT(std::abs(tr.radius - gt.radius), required ? 0.01 : 0.03) << id;
    EXPECT_LT(std::abs(tr.position.z() - gt.position.z()), required ? 0.02 : 0.05) << id;
    EXPECT_NEAR(tr.tilt_deg, gt.tilt_deg, 2.0) << id;
  }
  for (std::size_t i = 0; i < truth.size(); ++i)
    if (truth[i].tilt_deg <= max_required_tilt)
      EXPECT_TRUE(found[i]) << ctx << ": missed stem " << i << " (tilt " << truth[i].tilt_deg << ")";
}

class SyntheticVersions : public ::testing::TestWithParam<RetentionKind> {};

// Improved profile (default config: extra checks ON), verticality > 0.9 (paper).
// Vertical and 10-degree stems must be recovered. The 15-degree stem keeps only narrow side strips after the
// verticality filter (|n_z| < 0.1 <=> normal within 5.7 deg of horizontal);
// it must not produce false positives, but it is not required to be found.
TEST_P(SyntheticVersions, RecoversStemsAtPaperThreshold) {
  const auto& s = scene();
  const auto m = RetentionModel::make(GetParam(), s.cfg.retention);
  for (std::uint64_t seed : {1, 2, 3})
    checkRun(runStages(s.pre, m, seed, s.cfg), s.truth, 10.0,
             toString(GetParam()) + " seed " + std::to_string(seed));
}

// Baseline profile (extra checks OFF). Required stems (vertical, 10 deg) must
// still be recovered accurately; detections from the 15-degree stem's strips
// are allowed here and only counted - they are the reason the checks exist.
TEST_P(SyntheticVersions, BaselineProfileRequiredStems) {
  const auto& s = scene();
  Config cfg = s.cfg;
  cfg.cylinder.check_normal_consistency = cfg.cylinder.check_arc_coverage = false;
  const auto m = RetentionModel::make(GetParam(), cfg.retention);
  std::size_t extra_total = 0;
  for (std::uint64_t seed : {1, 2, 3}) {
    const RunResult r = runStages(s.pre, m, seed, cfg);
    std::vector<Eigen::Vector2d> ref;
    for (const auto& t : s.truth) ref.emplace_back(t.position.x(), t.position.y());
    for (std::size_t i = 0; i < s.truth.size(); ++i) {
      if (s.truth[i].tilt_deg > 10.0) continue;
      double best = 1e9;
      const TreeRecord* bt = nullptr;
      for (const auto& t : r.trees) {
        const double d = (t.position.head<2>() - ref[i]).norm();
        if (d < best) { best = d; bt = &t; }
      }
      const std::string id = toString(GetParam()) + " seed " + std::to_string(seed) + " stem " + std::to_string(i);
      ASSERT_NE(bt, nullptr) << id;
      EXPECT_LT(best, 0.02) << id;
      EXPECT_LT(std::abs(bt->radius - s.truth[i].radius), 0.01) << id;
    }
    std::size_t near_required = 0;
    for (const auto& t : r.trees)
      for (std::size_t i = 0; i < s.truth.size(); ++i)
        if (s.truth[i].tilt_deg <= 10.0 && (t.position.head<2>() - ref[i]).norm() < 0.5) ++near_required;
    EXPECT_EQ(near_required, 4u) << "baseline: duplicates at required stems";
    extra_total += r.trees.size() - near_required;
  }
  RecordProperty("baseline_detections_not_at_required_stems", int(extra_total));
  std::printf("[ info ] baseline %s: %zu detections from the 15-deg stem strips over 3 seeds\n",
              toString(GetParam()).c_str(), extra_total);
}

INSTANTIATE_TEST_SUITE_P(AllVersions, SyntheticVersions,
                         ::testing::Values(RetentionKind::Step, RetentionKind::LinearA, RetentionKind::LinearMid,
                                           RetentionKind::Physical),
                         [](const auto& info) { return toString(info.param); });

// With a looser verticality threshold (0.8 <=> 11.5 deg) the 15-degree stem is
// recovered too: shows the miss above comes from the paper's threshold, not
// from the tilted-axis / DTM-intersection logic.
TEST(Synthetic, LooserVerticalityRecoversAllStems) {
  const auto& s = scene();
  Config cfg = s.cfg;
  cfg.normals.vert_threshold = 0.8;
  const Preprocessed pre = preprocess(prepareCloud(makeSynthetic(SynthParams::defaultScene(), nullptr), cfg.scanner), cfg);
  for (std::uint64_t seed : {1, 2, 3})
    checkRun(runStages(pre, RetentionModel::make(RetentionKind::Step, cfg.retention), seed, cfg), s.truth, 90.0,
             "vert0.8 seed " + std::to_string(seed));
}

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
  EXPECT_EQ(mt.pairs.size(), 4u);  // all but the 15-degree stem (see above)
  EXPECT_EQ(mt.pairs.size(), r.trees.size());
  EXPECT_LT(mt.rmse(), 0.02);
}
