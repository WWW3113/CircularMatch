// Distance-dependent retention probability P(d) and common random numbers.
//
// P(d) is the probability of KEEPING a stem point at distance d from the
// scanner (near: keep fewer, far: keep more, because TLS density falls with d).
//
//  step        d<5 -> 0.5 ; 5<=d<10 -> 0.75 ; d>=10 -> 1
//              The paper uses open intervals, so d = 0, 5, 10 are undefined
//              there; the closed/open ends above are OUR completion.
//  linear_a    min(1, 0.5 + 0.05 d)        (our modification)
//              Equals step at the start of each step segment (d = 0, 5, 10) and
//              is >= step for every d, so it always keeps at least as many points.
//  linear_mid  min(1, 0.375 + 0.05 d)      (our modification)
//              Matches the step's segment MEANS on [0,5) and [5,10) only if points
//              were uniformly distributed in d; differs for d >= 10. Real clouds are
//              not uniform in d, so the kept count is NOT the same as step.
//  physical    clip((d/d0)^2, P_min, 1), d0 = 10 m, P_min = 0.1 (our modification)
//              Assumes TLS density ~ 1/d^2, so kept density is roughly uniform in d.
//  none        1 for every d: NO distance-dependent down-sampling (comparison only;
//              removes a step of the paper). Not part of cm_compare's four versions.
//
// Never assume two versions keep the same number of points: always measure.
//
// Common random numbers: every point gets u = hash(seed, quantised xyz) in
// [0,1), shared by all versions; a point is kept iff u < P(d). Hence version
// differences come only from P(d), and P_A <= P_B everywhere implies
// kept(A) is a subset of kept(B). u does not depend on processing order.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "cm/config.hpp"
#include "cm/types.hpp"

namespace cm {

enum class RetentionKind { Step, LinearA, LinearMid, Physical, None };

RetentionKind parseRetentionKind(const std::string& s);
std::string toString(RetentionKind k);

struct RetentionModel {
  RetentionKind kind = RetentionKind::Step;
  double intercept = 0.5;  // linear kinds
  double slope = 0.05;     // linear kinds
  double d0 = 10.0;        // physical
  double p_min = 0.1;      // physical
  std::string label;       // e.g. "step", "linear_a", "linear*" (budget-matched)

  double operator()(double d) const;  // always within [0, 1]
  static RetentionModel make(RetentionKind k, const RetentionParams& p);
  std::string describe() const;
};

std::uint64_t splitmix64(std::uint64_t x);
double commonUniform(std::uint64_t seed, const PointT& p, double quantum);

// Distance to the scanner in the local frame (scanner at origin).
inline double scannerDistance(const PointT& p, bool use_3d) {
  const double h2 = double(p.x) * p.x + double(p.y) * p.y;
  return std::sqrt(use_3d ? h2 + double(p.z) * p.z : h2);
}

// Per-1 m-bin comparison of empirical keep rate with mean theoretical P(d).
struct RetentionBin {
  double d_lo, d_hi;
  std::size_t n = 0, kept = 0;
  double p_mean = 0;  // mean P(d) of points in the bin
};
std::vector<RetentionBin> retentionBins(const std::vector<double>& d, const std::vector<char>& kept,
                                        const RetentionModel& m, double bin = 1.0);

}  // namespace cm
