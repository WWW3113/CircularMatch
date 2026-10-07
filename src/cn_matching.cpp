#include "cm/cn_matching.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace cm {

namespace {
double d2(const Eigen::Vector3d& a, const Eigen::Vector3d& b) { return std::hypot(a.x() - b.x(), a.y() - b.y()); }
}  // namespace

bool cnGatekeeperPass(const CnDescriptor& a, const CnDescriptor& b, double thr) {
  for (int k = 0; k < 3; ++k)
    if (std::abs(a.val[k] - b.val[k]) < thr) return true;  // D3: one consistent bit is enough
  return false;
}

int cnDcScore(const CnDescriptor& a, const CnDescriptor& b, double thr, std::vector<std::pair<int, int>>* ip) {
  int dc = 0;
  if (ip) ip->clear();
  for (std::size_t s = 3; s < a.val.size(); ++s) {
    if (a.idx[s] < 0 || b.idx[s] < 0) continue;  // D2: empty never matches
    if (std::abs(a.val[s] - b.val[s]) < thr) {
      ++dc;
      if (ip) ip->emplace_back(a.idx[s], b.idx[s]);
    }
  }
  return dc;
}

std::vector<std::pair<int, int>> cnTriangleMatch(const std::vector<int>& S, const std::vector<int>& T,
                                                 const std::vector<Eigen::Vector3d>& skp,
                                                 const std::vector<Eigen::Vector3d>& tkp, double thr,
                                                 std::size_t* n_tri) {
  // Votes for vertex correspondences from every matching triangle pair.
  std::map<std::pair<int, int>, int> votes;
  std::size_t ntri = 0;
  static const int perm[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
  const int ns = static_cast<int>(S.size()), nt = static_cast<int>(T.size());
  for (int a = 0; a < ns; ++a)
    for (int b = a + 1; b < ns; ++b)
      for (int c = b + 1; c < ns; ++c) {
        const std::array<int, 3> sv{{S[a], S[b], S[c]}};
        // side opposite vertex k: between the other two
        const std::array<double, 3> ss{{d2(skp[sv[1]], skp[sv[2]]), d2(skp[sv[0]], skp[sv[2]]),
                                        d2(skp[sv[0]], skp[sv[1]])}};
        for (int x = 0; x < nt; ++x)
          for (int y = x + 1; y < nt; ++y)
            for (int z = y + 1; z < nt; ++z) {
              const std::array<int, 3> tv{{T[x], T[y], T[z]}};
              const std::array<double, 3> ts{{d2(tkp[tv[1]], tkp[tv[2]]), d2(tkp[tv[0]], tkp[tv[2]]),
                                              d2(tkp[tv[0]], tkp[tv[1]])}};
              for (const auto& pm : perm) {  // source vertex k <-> target vertex pm[k]
                if (std::abs(ss[0] - ts[pm[0]]) < thr && std::abs(ss[1] - ts[pm[1]]) < thr &&
                    std::abs(ss[2] - ts[pm[2]]) < thr) {
                  ++ntri;
                  for (int k = 0; k < 3; ++k) ++votes[{sv[k], tv[pm[k]]}];
                  break;  // first matching correspondence (fixed order)
                }
              }
            }
      }
  if (n_tri) *n_tri = ntri;
  // D13: one-to-one by descending votes (ties -> lower indices).
  std::vector<std::tuple<int, int, int>> v;
  for (const auto& [k, n] : votes) v.emplace_back(-n, k.first, k.second);
  std::sort(v.begin(), v.end());
  std::set<int> us, ut;
  std::vector<std::pair<int, int>> out;
  for (const auto& [nn, s, t] : v) {
    if (us.count(s) || ut.count(t)) continue;
    us.insert(s);
    ut.insert(t);
    out.emplace_back(s, t);
  }
  return out;
}

CnMatchResult matchCn(const std::vector<CnDescriptor>& src, const std::vector<CnDescriptor>& tgt,
                      const std::vector<Eigen::Vector3d>& skp, const std::vector<Eigen::Vector3d>& tkp,
                      const CnParams& p) {
  CnMatchResult r;
  const double thr = p.threshold;
  // Alg. 1 l. 3-14: best target (max DC) for every source descriptor.
  std::map<int, std::vector<std::pair<int, int>>> candidate;  // target -> (source, DC)
  for (const auto& Di : src) {
    if (!Di.valid) continue;
    int best_dc = -1, best_j = -1;
    for (const auto& Dj : tgt) {
      if (!Dj.valid || !cnGatekeeperPass(Di, Dj, thr)) continue;
      ++r.gate_passed;
      const int dc = cnDcScore(Di, Dj, thr);
      if (dc > best_dc) {  // strict: ties keep the lower target index
        best_dc = dc;
        best_j = Dj.center;
      }
    }
    if (best_j >= 0) candidate[best_j].emplace_back(Di.center, best_dc);
  }
  // l. 15-21: best source per target; keep if DC >= min_dc; update FM.
  struct Cand {
    int i, j, dc;
    std::vector<std::pair<int, int>> ip;
  };
  std::vector<Cand> cands;
  std::map<std::pair<int, int>, int> fm;
  for (const auto& [j, list] : candidate) {
    int bi = -1, bdc = -1;
    for (const auto& [i, dc] : list)
      if (dc > bdc || (dc == bdc && i < bi)) {
        bdc = dc;
        bi = i;
      }
    if (bdc < p.min_dc) continue;
    Cand c{bi, j, bdc, {}};
    cnDcScore(src[bi], tgt[j], thr, &c.ip);
    for (const auto& pr : c.ip) ++fm[pr];
    cands.push_back(std::move(c));
  }
  r.candidates = cands.size();
  if (cands.empty()) return r;
  // l. 22-25: MatchScore = w * FM + DC, sorted descending (ties -> higher DC, lower indices).
  std::vector<std::tuple<double, int, int, int, std::size_t>> scored;
  for (std::size_t k = 0; k < cands.size(); ++k) {
    const auto it = fm.find({cands[k].i, cands[k].j});
    const int f = it == fm.end() ? 0 : it->second;
    scored.emplace_back(p.w * f + cands[k].dc, cands[k].dc, f, -cands[k].i, k);
  }
  std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
    if (std::get<0>(a) != std::get<0>(b)) return std::get<0>(a) > std::get<0>(b);
    if (std::get<1>(a) != std::get<1>(b)) return std::get<1>(a) > std::get<1>(b);
    return std::get<3>(a) > std::get<3>(b);
  });
  r.best_score = std::get<0>(scored[0]);
  r.best_dc = std::get<1>(scored[0]);
  r.best_fm = std::get<2>(scored[0]);
  // l. 26-31
  for (std::size_t k = 0; k < scored.size(); ++k) {
    const double s = std::get<0>(scored[k]);
    const Cand& c = cands[std::get<4>(scored[k])];
    if (k == 0 && s < p.th_score) {
      std::vector<int> S, T;
      for (const auto& [a, b] : c.ip) {
        if (std::find(S.begin(), S.end(), a) == S.end()) S.push_back(a);
        if (std::find(T.begin(), T.end(), b) == T.end()) T.push_back(b);
      }
      r.triangle_set_src = S.size();
      r.triangle_set_tgt = T.size();
      r.pairs = cnTriangleMatch(S, T, skp, tkp, thr, &r.triangle_matches);
      r.used_triangle = true;
    } else if (s > p.th_score) {
      r.pairs.emplace_back(c.i, c.j);
    }
  }
  return r;
}

}  // namespace cm
