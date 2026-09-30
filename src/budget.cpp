#include "cm/budget.hpp"

#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace cm {

std::size_t countKept(const RetentionModel& m, const std::vector<double>& d, const std::vector<double>& u) {
  std::size_t n = 0;
  for (std::size_t i = 0; i < d.size(); ++i) n += u[i] < m(d[i]) ? 1 : 0;
  return n;
}

BudgetResult matchBudget(const RetentionModel& base, const std::vector<double>& d, const std::vector<double>& u,
                         std::size_t target, const BudgetParams& p) {
  if (base.kind == RetentionKind::Step) throw std::runtime_error("budget matching is not defined for step");
  const double tol = std::max<double>(p.abs_tol, p.rel_tol * double(target));
  const bool linear = base.kind != RetentionKind::Physical;
  // Parameter t: linear -> intercept (count increasing); physical -> log(d0) (count decreasing).
  auto modelAt = [&](double t) {
    RetentionModel m = base;
    if (linear) m.intercept = t;
    else m.d0 = std::exp(t);
    return m;
  };
  double lo = linear ? p.intercept_lo : std::log(p.d0_lo);
  double hi = linear ? p.intercept_hi : std::log(p.d0_hi);
  // Orient so that count(lo) <= count(hi).
  if (!linear) std::swap(lo, hi);

  BudgetResult best;
  best.target = target;
  double best_diff = INFINITY;
  auto consider = [&](double t, std::size_t k) {
    const double diff = std::abs(double(k) - double(target));
    if (diff < best_diff) {
      best_diff = diff;
      best.model = modelAt(t);
      best.kept = k;
    }
  };
  const std::size_t klo = countKept(modelAt(lo), d, u), khi = countKept(modelAt(hi), d, u);
  consider(lo, klo);
  consider(hi, khi);
  int it = 0;
  if (klo <= target && target <= khi) {
    for (; it < p.max_iter && best_diff > tol; ++it) {
      const double mid = 0.5 * (lo + hi);
      const std::size_t k = countKept(modelAt(mid), d, u);
      consider(mid, k);
      if (k < target) lo = mid;
      else hi = mid;
    }
  }
  best.iterations = it;
  best.converged = best_diff <= tol;
  best.model.label = linear ? "linear*" : "physical*";
  return best;
}

}  // namespace cm
