#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace cm {
// Smallest circular interval containing ceil(fraction*N) observed inlier angles.
// Unlike occupied-bin count, this measures extent without requiring dense bins.
// A trimmed interval prevents a few isolated angular outliers inflating extent.
inline double trimmedArcDegrees(std::vector<double> angles, double fraction=0.90) {
  if (!(fraction>0 && fraction<=1)) throw std::invalid_argument("invalid arc support fraction");
  if (angles.size()<2) return 0;
  const double tau=2*std::acos(-1.0);
  for(auto& a:angles) {
    if(!std::isfinite(a))throw std::invalid_argument("nonfinite arc angle");
    a=std::fmod(a,tau);if(a<0)a+=tau;
  }
  std::sort(angles.begin(),angles.end());const auto n=angles.size();
  const auto keep=std::max<std::size_t>(2,std::size_t(std::ceil(fraction*n)));
  for(std::size_t i=0;i<n;++i)angles.push_back(angles[i]+tau);
  double best=tau;
  for(std::size_t i=0;i<n;++i)best=std::min(best,angles[i+keep-1]-angles[i]);
  return best*180/std::acos(-1.0);
}
}  // namespace cm
