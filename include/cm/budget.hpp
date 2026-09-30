// Budget matching: tune one parameter so that the ACTUAL number of kept points
// equals a target (the step version's kept count for the same seed), within
// max(abs_tol, rel_tol * target). This separates "shape of P(d)" from "how
// many points are kept".
//   linear kinds : tune the intercept (kept count is non-decreasing in it)
//   physical     : tune d0 (kept count is non-increasing in it), P_min fixed
// Note: linear_a and linear_mid differ only by intercept, so after matching
// they become the SAME function; the matched table shows one column "linear*".
#pragma once
#include <vector>

#include "cm/config.hpp"
#include "cm/retention.hpp"

namespace cm {

struct BudgetResult {
  RetentionModel model;
  std::size_t kept = 0, target = 0;
  int iterations = 0;
  bool converged = false;  // |kept - target| within tolerance
};

std::size_t countKept(const RetentionModel& m, const std::vector<double>& d, const std::vector<double>& u);

BudgetResult matchBudget(const RetentionModel& base, const std::vector<double>& d, const std::vector<double>& u,
                         std::size_t target, const BudgetParams& p);

}  // namespace cm
