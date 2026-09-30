// One-to-one matching of detected vs reference tree positions.
// Only pairs with horizontal distance <= radius are admissible. Among all
// matchings we take one with the MAXIMUM number of pairs and, among those,
// the MINIMUM total horizontal distance (Hungarian algorithm with a large
// penalty for inadmissible pairs). Unlike greedy matching the result does not
// depend on processing order.
#pragma once
#include <Eigen/Core>
#include <utility>
#include <vector>

namespace cm {

struct MatchResult {
  std::vector<std::pair<int, int>> pairs;  // (detected index, reference index)
  std::vector<double> errors;              // horizontal distance per pair
  std::size_t n_detected = 0, n_reference = 0;
  double recall() const { return n_reference ? double(pairs.size()) / double(n_reference) : 0.0; }
  double precision() const { return n_detected ? double(pairs.size()) / double(n_detected) : 0.0; }
  double mean_error() const;
  double rmse() const;
};

MatchResult matchPositions(const std::vector<Eigen::Vector2d>& detected, const std::vector<Eigen::Vector2d>& reference,
                           double radius);

// Rectangular assignment, rows <= cols; returns col index per row. Exposed for tests.
std::vector<int> hungarian(const std::vector<std::vector<double>>& cost);

}  // namespace cm
