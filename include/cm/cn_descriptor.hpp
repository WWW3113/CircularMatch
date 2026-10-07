// Circular-neighbourhood (CN) descriptor, paper Section II-A, eq. (6), Figs. 2-3.
//
// For keypoint k_i (= p0) with its three nearest keypoints p1, p2, p3:
//  - encode e (e = 1, 2, 3) uses the direction p0 -> p_e as the start direction and
//    divides the surroundings into N - 3 equal sectors counter-clockwise (eq. 6);
//    sector 0 contains p_e. In every sector the keypoint closest to p0 is taken:
//    its distance (row 2(e-1)) and index (row 2(e-1)+1) form the 6 x (N - 3)
//    feature matrix M.
//  - descriptor (N values): dims 0-2 are the gatekeeper bits = distances to
//    p1, p2, p3 (with their indices); dims 3..N-1 take, for each sector s, the
//    encode1 entry, else encode2, else encode3, else empty (Fig. 3).
//
// Decisions where the paper is silent (see docs/cn_ambiguities.md):
//  D1  all distances and angles are 2-D (x, y); z is ignored.
//  D2  an empty dimension stores value 0 and index -1, and never matches.
//  D12 sector 0 of encode2 / encode3 may contain p1 (or p2) as the closest point;
//      taken literally (closest point per sector) and counted in CnStats.
//  D14 all other keypoints of the scan are candidates (no neighbourhood radius).
#pragma once
#include <Eigen/Core>
#include <array>
#include <cstddef>
#include <vector>

namespace cm {

struct CnParams {
  int N = 183;              // paper: descriptor length
  double threshold = 0.05;  // paper: distance threshold for all descriptor comparisons [m]
  double w = 10.0;          // paper: FM weight in MatchScore = w * FM + DC (eq. 7)
  double th_score = 40.0;   // paper: matching threshold
  int min_dc = 3;           // D4: keep candidate pairs with DC >= min_dc (text: "below 3" filtered)
};

struct CnDescriptor {
  int center = -1;
  std::array<int, 3> nb{{-1, -1, -1}};  // p1, p2, p3 keypoint indices
  std::vector<double> val;              // N values; 0 = empty
  std::vector<int> idx;                 // N keypoint indices; -1 = empty
  bool valid = false;                   // false if fewer than 4 keypoints are available
};

// 6 x (N - 3) feature matrix of one keypoint, exposed for tests.
struct CnFeatureMatrix {
  std::array<std::vector<double>, 3> dist;  // encode1..3 distance rows
  std::array<std::vector<int>, 3> index;    // encode1..3 index rows (-1 = empty)
};

struct CnStats {
  std::size_t descriptors = 0;
  std::size_t enc2_sector0_is_p1 = 0;        // D12: encode2 sector 0 closest point is p1
  std::size_t enc3_sector0_is_p1_or_p2 = 0;  // D12: encode3 sector 0 closest point is p1 or p2
  std::size_t coincident_skipped = 0;        // keypoints at the same 2-D position as p0 (no angle)
};

// Angle of v relative to the start direction a, counter-clockwise, in [0, 2 pi) (eq. 6, 2-D).
double cnAngle(const Eigen::Vector2d& a, const Eigen::Vector2d& v);

CnFeatureMatrix cnFeatureMatrix(const std::vector<Eigen::Vector3d>& kp, int center, const std::array<int, 3>& nb,
                                const CnParams& p, CnStats* stats = nullptr);

std::vector<CnDescriptor> buildCnDescriptors(const std::vector<Eigen::Vector3d>& kp, const CnParams& p,
                                             CnStats* stats = nullptr);

// D9 (option, default off): merge keypoints closer than `radius` (2-D) into their mean.
std::vector<Eigen::Vector3d> mergeKeypoints(const std::vector<Eigen::Vector3d>& kp, double radius);

}  // namespace cm
