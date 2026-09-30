#include <gtest/gtest.h>

#include <cmath>

#include "cm/budget.hpp"
#include "cm/pipeline.hpp"
#include "cm/synth.hpp"

using namespace cm;

namespace {
// d distributed like a TLS scan (dense near, sparse far), u from the hash.
void makeDU(std::size_t n, std::vector<double>* d, std::vector<double>* u) {
  std::uint64_t s = 99;
  for (std::size_t i = 0; i < n; ++i) {
    s = splitmix64(s);
    const double a = (s >> 11) * 0x1.0p-53;
    d->push_back(1.0 + 25.0 * a * a);  // non-uniform in d
    s = splitmix64(s);
    u->push_back((s >> 11) * 0x1.0p-53);
  }
}
}  // namespace

TEST(Budget, MatchesStepCountWithinTolerance) {
  std::vector<double> d, u;
  makeDU(200000, &d, &u);
  RetentionParams rp;
  BudgetParams bp;
  const std::size_t target = countKept(RetentionModel::make(RetentionKind::Step, rp), d, u);
  const double tol = std::max<double>(bp.abs_tol, bp.rel_tol * target);
  for (auto k : {RetentionKind::LinearA, RetentionKind::LinearMid, RetentionKind::Physical}) {
    const auto raw = countKept(RetentionModel::make(k, rp), d, u);
    const auto b = matchBudget(RetentionModel::make(k, rp), d, u, target, bp);
    EXPECT_TRUE(b.converged) << toString(k);
    EXPECT_LE(std::abs(double(b.kept) - double(target)), tol) << toString(k);
    EXPECT_EQ(b.kept, countKept(b.model, d, u)) << "reported count must be the actual count";
    // Raw versions differ from step (they are not budget-equal by construction).
    EXPECT_NE(raw, target) << toString(k);
  }
}

TEST(Budget, LinearAAndMidCoincideAfterMatching) {
  std::vector<double> d, u;
  makeDU(100000, &d, &u);
  RetentionParams rp;
  const std::size_t target = countKept(RetentionModel::make(RetentionKind::Step, rp), d, u);
  const auto a = matchBudget(RetentionModel::make(RetentionKind::LinearA, rp), d, u, target, BudgetParams{});
  const auto m = matchBudget(RetentionModel::make(RetentionKind::LinearMid, rp), d, u, target, BudgetParams{});
  EXPECT_NEAR(a.model.intercept, m.model.intercept, 1e-6);
}

TEST(Budget, OnSyntheticScenePipeline) {
  const RawCloud raw = makeSynthetic(SynthParams::defaultScene(), nullptr);
  Config cfg;
  const Preprocessed pre = preprocess(prepareCloud(raw, cfg.scanner), cfg);
  for (std::uint64_t seed = 1; seed <= 3; ++seed) {
    const auto u = computeUniforms(pre, seed, cfg);
    const auto step = runStages(pre, RetentionModel::make(RetentionKind::Step, cfg.retention), seed, cfg, &u);
    const double tol = std::max<double>(cfg.budget.abs_tol, cfg.budget.rel_tol * step.kept);
    for (auto k : {RetentionKind::LinearA, RetentionKind::Physical}) {
      const auto b = matchBudget(RetentionModel::make(k, cfg.retention), pre.dist, u, step.kept, cfg.budget);
      const auto r = runStages(pre, b.model, seed, cfg, &u);
      EXPECT_LE(std::abs(double(r.kept) - double(step.kept)), tol) << toString(k) << " seed " << seed;
    }
  }
}
