#include "cm/cn_recovery.hpp"
#include "cm/registration.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace cm {
namespace {
using Pair = std::pair<int,int>;
std::vector<Pair> votedPairs(const std::map<Pair,int>& votes, std::size_t limit) {
  std::vector<std::tuple<int,int,int>> sorted;
  for (const auto& [p,n] : votes) sorted.emplace_back(-n,p.first,p.second);
  std::sort(sorted.begin(),sorted.end());
  std::vector<Pair> out;
  for (const auto& [n,i,j] : sorted) {
    if (out.size()==limit) break;
    out.emplace_back(i,j);
  }
  return out;
}
std::vector<Pair> oneToOne(const std::vector<Pair>& ordered) {
  std::set<int> src,tgt;
  std::vector<Pair> out;
  for (const auto& [i,j] : ordered) if (!src.count(i) && !tgt.count(j)) {
    src.insert(i); tgt.insert(j); out.emplace_back(i,j);
  }
  return out;
}
}  // namespace

CnRecoveryResult recoverCn(const CnMatchResult& initial,
                          const std::vector<Eigen::Vector3d>& source,
                          const std::vector<Eigen::Vector3d>& target,
                          const CnParams& cn, const CnRecoveryParams& p) {
  CnRecoveryResult out;
  out.pairs=initial.pairs;
  if (p.mode==CnRecoveryMode::Off || initial.pairs.size()>=p.trigger_matches) return out;
  if (p.top_candidates==0 || p.max_evidence_pairs<3 || p.min_support<3 ||
      !(p.inlier_distance>0) || !std::isfinite(p.inlier_distance))
    throw std::invalid_argument("invalid CN recovery parameters");
  out.triggered=true;
  const auto count=std::min(p.top_candidates,initial.ranked_candidates.size());
  std::map<Pair,int> votes;
  for (std::size_t k=0; k<count; ++k) {
    const auto& c=initial.ranked_candidates[k];
    if (p.mode==CnRecoveryMode::Triangles) {
      std::set<int> ss{c.source},tt{c.target};
      for (const auto& [i,j] : c.evidence) { ss.insert(i); tt.insert(j); }
      std::size_t n=0;
      const auto pairs=cnTriangleMatch({ss.begin(),ss.end()},{tt.begin(),tt.end()},source,target,cn.threshold,&n);
      out.hypotheses+=n;
      for (const auto& pair : pairs) ++votes[pair];
    } else {
      std::set<Pair> unique(c.evidence.begin(),c.evidence.end());
      unique.emplace(c.source,c.target);
      for (const auto& pair : unique) ++votes[pair];
    }
  }
  if (p.mode==CnRecoveryMode::Triangles) {
    auto pairs=oneToOne(votedPairs(votes,votes.size()));
    out.support=pairs.size();
    if (pairs.size()>=3 && pairs.size()>initial.pairs.size()) {
      out.pairs=std::move(pairs); out.accepted=true;
    }
    return out;
  }
  const auto pool=votedPairs(votes,p.max_evidence_pairs);
  // Enumerate bounded triples of descriptor-supported correspondences, not
  // arbitrary scan points. Check horizontal lengths, estimate full 6-DOF,
  // and score only the same evidence pool using full 3-D residuals.
  auto compatible=[&](std::size_t a,std::size_t b) {
    const auto [i,j]=pool[a];const auto [u,v]=pool[b];
    return i!=u && j!=v && std::abs((source[i]-source[u]).head<2>().norm()-
                                  (target[j]-target[v]).head<2>().norm())<2*cn.threshold;
  };
  double best_median=std::numeric_limits<double>::infinity(),best_sum=best_median;
  std::vector<Pair> best;
  for (std::size_t a=0; a<pool.size(); ++a)
    for (std::size_t b=a+1; b<pool.size(); ++b) {
      if (!compatible(a,b)) continue;
      for (std::size_t c=b+1; c<pool.size(); ++c) {
        if (!compatible(a,c) || !compatible(b,c)) continue;
        const std::array<Pair,3> triple{{pool[a],pool[b],pool[c]}};
        std::vector<Eigen::Vector3d> s,t;
        for (const auto& [i,j] : triple) { s.push_back(source[i]);t.push_back(target[j]); }
        const auto pose=kabsch(s,t);
        if (!pose) continue;
        ++out.hypotheses;
        std::vector<std::tuple<double,int,int>> residuals;
        for (const auto& [i,j] : pool) {
          const double d=(pose->apply(source[i])-target[j]).norm();
          if (d<p.inlier_distance) residuals.emplace_back(d,i,j);
        }
        std::sort(residuals.begin(),residuals.end());
        std::set<int> us,ut;std::vector<Pair> in;std::vector<double> errors;double sum=0;
        for (const auto& [d,i,j] : residuals) if (!us.count(i) && !ut.count(j)) {
          us.insert(i);ut.insert(j);in.emplace_back(i,j);errors.push_back(d);sum+=d;
        }
        if (in.empty()) continue;
        const std::size_t n=errors.size();
        const double median=n%2 ? errors[n/2] : (errors[n/2-1]+errors[n/2])/2;
        if (in.size()>best.size() || (in.size()==best.size() &&
            (median<best_median || (median==best_median && sum<best_sum)))) {
          best=std::move(in);best_median=median;best_sum=sum;
        }
      }
    }
  out.support=best.size();
  if (!best.empty()) out.median_residual=best_median;
  if (best.size()>=p.min_support && best.size()>initial.pairs.size()) {
    out.pairs=std::move(best);out.accepted=true;
  }
  return out;
}
}  // namespace cm
