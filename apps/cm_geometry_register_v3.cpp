// GT-free coarse hypotheses and robust full-6DOF surface refinement.
#include <Eigen/Geometry>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include "app_common.hpp"
#include "cm/cn_recovery.hpp"
#include "cm/geometry_refinement.hpp"
#include "cm/io.hpp"
#include "cm/normals.hpp"
#include "cm/sanity.hpp"
#include "cm/types.hpp"
using namespace cm;
namespace {
std::vector<Eigen::Vector3d> trees(const std::string& path) {
  std::ifstream in(path);if(!in)throw std::runtime_error("cannot read trees");std::string line;std::getline(in,line);
  std::vector<std::string> headers;std::stringstream hs(line);std::string s;while(std::getline(hs,s,','))headers.push_back(s);
  std::array<int,3> columns;for(int k=0;k<3;++k){auto it=std::find(headers.begin(),headers.end(),std::string(1,"xyz"[k]));if(it==headers.end())throw std::runtime_error("tree coordinate missing");columns[k]=int(it-headers.begin());}
  std::vector<Eigen::Vector3d> out;while(std::getline(in,line))if(!line.empty()) {
    std::stringstream ss(line);std::vector<std::string> cells;while(std::getline(ss,s,','))cells.push_back(s);
    Eigen::Vector3d p;for(int k=0;k<3;++k)p[k]=std::stod(cells.at(columns[k]));if(!p.allFinite())throw std::runtime_error("nonfinite tree");out.push_back(p);
  }return out;
}
void save(const std::string& path,const std::vector<SurfaceLevel>& levels) {
  std::ofstream out(path,std::ios::binary);out.write("CMGEO001",8);
  for(const auto& level:levels) {
    const std::uint64_t n=level.points.size();out.write(reinterpret_cast<const char*>(&level.voxel),8);out.write(reinterpret_cast<const char*>(&n),8);
    for(const auto& q:level.points){float a[6];for(int k=0;k<3;++k){a[k]=q.p[k];a[k+3]=q.n[k];}out.write(reinterpret_cast<const char*>(a),24);}
  }if(!out)throw std::runtime_error("cache write failed");
}
std::vector<SurfaceLevel> load(const std::string& path) {
  std::ifstream in(path,std::ios::binary);char magic[8];in.read(magic,8);if(!in||std::string(magic,8)!="CMGEO001")throw std::runtime_error("invalid geometry cache");
  std::vector<SurfaceLevel> levels(3);for(auto& level:levels) {
    std::uint64_t n=0;in.read(reinterpret_cast<char*>(&level.voxel),8);in.read(reinterpret_cast<char*>(&n),8);if(!in||n>20000000||!(level.voxel>0))throw std::runtime_error("invalid cache size");level.points.resize(n);
    for(auto& q:level.points){float a[6];in.read(reinterpret_cast<char*>(a),24);for(int k=0;k<3;++k){q.p[k]=a[k];q.n[k]=a[k+3];}if(!in||!q.p.allFinite()||!q.n.allFinite())throw std::runtime_error("invalid surface point");q.n.normalize();}
  }return levels;
}
void prepare(const std::string& input,const std::string& output) {
  Timer whole,readtimer;const auto raw=readPointCloud(input);const double read_ms=readtimer.ms();std::vector<SurfaceLevel> levels;
  std::ofstream stats(output+".timing.csv");stats<<"voxel_m,raw_points,centroids,valid_planar_points,read_ms,voxel_ms,normals_ms\n";
  for(double voxel:{.5,.2,.1}) {
    Timer vt;using Key=std::tuple<int,int,int>;struct Bin {Eigen::Vector3d sum=Eigen::Vector3d::Zero();int n=0;};
    struct Hash {std::size_t operator()(const Key& k)const{return std::uint32_t(std::get<0>(k))*73856093u^std::uint32_t(std::get<1>(k))*19349663u^std::uint32_t(std::get<2>(k))*83492791u;}};
    std::unordered_map<Key,Bin,Hash> bins;for(const auto& p:raw.points){auto& b=bins[{int(std::floor(p.x()/voxel)),int(std::floor(p.y()/voxel)),int(std::floor(p.z()/voxel))}];b.sum+=p;++b.n;}
    std::vector<Key> keys;keys.reserve(bins.size());for(const auto& [key,b]:bins)keys.push_back(key);std::sort(keys.begin(),keys.end());
    Cloud cloud;cloud.reserve(bins.size());for(const auto& key:keys){const auto& b=bins.at(key);const auto p=(b.sum/b.n).eval();cloud.push_back(PointT(p.x(),p.y(),p.z()));}
    const double voxel_ms=vt.ms();NormalParams np;np.radius=2.5*voxel;np.octree_res=voxel;np.min_neighbors=6;Timer nt;const auto normals=computeNormals(cloud,np);const double normal_ms=nt.ms();
    SurfaceLevel level;level.voxel=voxel;for(std::size_t i=0;i<cloud.size();++i)if(normals.valid[i]&&(*normals.normals)[i].curvature<=.1) {
      const auto& n=(*normals.normals)[i];level.points.push_back({cloud[i].getVector3fMap().cast<double>(),Eigen::Vector3d(n.normal_x,n.normal_y,n.normal_z).normalized()});
    }
    stats<<voxel<<','<<raw.points.size()<<','<<cloud.size()<<','<<level.points.size()<<','<<read_ms<<','<<voxel_ms<<','<<normal_ms<<'\n';levels.push_back(std::move(level));std::cout<<"prepared "<<voxel<<' '<<levels.back().points.size()<<std::endl;
  }
  save(output,levels);std::cout<<"total_prepare_ms "<<whole.ms()<<std::endl;
}
using Pair=std::pair<int,int>;
struct Proposal {RigidTransform pose;int support=0;double error=1e9;};
int globalSupport(const RigidTransform& pose,const std::vector<Eigen::Vector3d>& src,const std::vector<Eigen::Vector3d>& tgt,double* error=nullptr) {
  std::vector<std::tuple<double,int,int>> pairs;
  for(std::size_t i=0;i<src.size();++i){const auto p=pose.apply(src[i]);int best=-1;double distance=.3*.3;
    for(std::size_t j=0;j<tgt.size();++j){const double d=(p-tgt[j]).head<2>().squaredNorm();if(d<distance){distance=d;best=int(j);}}
    if(best>=0)pairs.emplace_back(distance,int(i),best);
  }
  std::sort(pairs.begin(),pairs.end());std::set<int> used;int n=0;double sum=0;for(const auto& [d,i,j]:pairs)if(used.insert(j).second){++n;sum+=d;}
  if(error)*error=sum;return n;
}
int support(const RigidTransform& pose,const std::vector<Pair>& pool,const std::vector<Eigen::Vector3d>& src,const std::vector<Eigen::Vector3d>& tgt,double* sum=nullptr) {
  std::vector<std::tuple<double,int,int>> residual;for(const auto& [i,j]:pool){const double d=(pose.apply(src[i])-tgt[j]).head<2>().norm();if(d<.3)residual.emplace_back(d,i,j);}
  std::sort(residual.begin(),residual.end());std::set<int> a,b;int n=0;double total=0;
  for(const auto& [d,i,j]:residual)if(!a.count(i)&&!b.count(j)){a.insert(i);b.insert(j);++n;total+=d;}
  if(sum)*sum=total;return n;
}
std::vector<Pair> evidence(const CnMatchResult& match) {
  std::map<Pair,int> votes;for(std::size_t k=0;k<std::min<std::size_t>(20,match.ranked_candidates.size());++k){const auto& c=match.ranked_candidates[k];std::set<Pair> unique(c.evidence.begin(),c.evidence.end());unique.insert({c.source,c.target});for(const auto& pair:unique)++votes[pair];}
  std::vector<std::tuple<int,int,int>> ranked;for(const auto& [p,n]:votes)ranked.emplace_back(-n,p.first,p.second);std::sort(ranked.begin(),ranked.end());std::vector<Pair> out;
  for(const auto& [n,i,j]:ranked){if(out.size()==120)break;out.emplace_back(i,j);}return out;
}
std::vector<Proposal> proposals(const CnMatchResult& match,const std::vector<Eigen::Vector3d>& src,const std::vector<Eigen::Vector3d>& tgt,const std::vector<Pair>& pool) {
  std::vector<Proposal> out;std::size_t tested=0;
  // Upright yaw seeds are initializations only: every accepted result is
  // subsequently estimated in full 6DOF, including roll/pitch and vertical shift.
  for(std::size_t a=0;a<pool.size();++a)for(std::size_t b=a+1;b<pool.size();++b) {
    const auto [i,j]=pool[a];const auto [u,v]=pool[b];if(i==u||j==v)continue;
    const Eigen::Vector2d ds=(src[u]-src[i]).head<2>(),dt=(tgt[v]-tgt[j]).head<2>();
    if(ds.norm()<1||std::abs(ds.norm()-dt.norm())>=.2)continue;
    const double angle=std::atan2(dt.y(),dt.x())-std::atan2(ds.y(),ds.x());
    RigidTransform pose;pose.R=Eigen::AngleAxisd(angle,Eigen::Vector3d::UnitZ()).toRotationMatrix();pose.t=((tgt[j]-pose.R*src[i])+(tgt[v]-pose.R*src[u]))/2;
    std::vector<double> heights;for(const auto& [s,t]:pool)if((pose.apply(src[s])-tgt[t]).head<2>().norm()<.3)heights.push_back(tgt[t].z()-(pose.R*src[s]).z());
    if(!heights.empty()){auto mid=heights.begin()+heights.size()/2;std::nth_element(heights.begin(),mid,heights.end());pose.t.z()=*mid;}
    Proposal candidate;candidate.pose=pose;candidate.support=globalSupport(pose,src,tgt,&candidate.error);if(candidate.support<3)continue;
    bool merged=false;for(auto& prior:out)if(rotationErrorDeg(prior.pose.R,pose.R)<5&&(prior.pose.t-pose.t).norm()<.5){if(candidate.support>prior.support||(candidate.support==prior.support&&candidate.error<prior.error))prior=candidate;merged=true;break;}
    if(!merged)out.push_back(candidate);
    std::sort(out.begin(),out.end(),[](const Proposal&a,const Proposal&b){return a.support!=b.support?a.support>b.support:a.error<b.error;});if(out.size()>256)out.resize(256);
  }
  auto compatible=[&](std::size_t a,std::size_t b){auto [i,j]=pool[a];auto [u,v]=pool[b];return i!=u&&j!=v&&std::abs((src[i]-src[u]).head<2>().norm()-(tgt[j]-tgt[v]).head<2>().norm())<.2;};
  for(std::size_t a=0;a<pool.size();++a)for(std::size_t b=a+1;b<pool.size();++b)if(compatible(a,b))for(std::size_t c=b+1;c<pool.size();++c)if(compatible(a,c)&&compatible(b,c)) {
    if(++tested>10000)return out;
    auto [i,j]=pool[a];auto [u,v]=pool[b];auto [s,t]=pool[c];const auto pose=kabsch({src[i],src[u],src[s]},{tgt[j],tgt[v],tgt[t]});if(!pose)continue;
    Proposal candidate;candidate.pose=*pose;candidate.support=globalSupport(*pose,src,tgt,&candidate.error);if(candidate.support<3)continue;
    bool merged=false;for(auto& prior:out)if(rotationErrorDeg(prior.pose.R,pose->R)<5&&(prior.pose.t-pose->t).norm()<.5) {
      if(candidate.support>prior.support||(candidate.support==prior.support&&candidate.error<prior.error))prior=candidate;merged=true;break;
    }
    if(!merged)out.push_back(candidate);
    std::sort(out.begin(),out.end(),[](const Proposal&a,const Proposal&b){return a.support!=b.support?a.support>b.support:a.error<b.error;});if(out.size()>256)out.resize(256);
  }return out;
}
void infer(const std::vector<SurfaceLevel>& source,const std::vector<SurfaceLevel>& target,const std::string& batch,const std::string& output) {
  std::ifstream jobs(batch);std::ofstream out(output);if(!jobs||!out)throw std::runtime_error("cannot open batch/output");out<<std::setprecision(17);
  out<<"label,method,pose_ok,descriptor_ms,matching_ms,solve_ms,refine_ms,online_ms,proposal_count,refined_candidates,geo_overlap,geo_rmse,iterations";
  for(int i=0;i<3;++i)for(int j=0;j<3;++j)out<<",r"<<i<<j;out<<",t0,t1,t2\n";
  std::string line;while(std::getline(jobs,line))if(!line.empty()) {
    std::stringstream ss(line);std::string method,sp,tp,label,extra;if(!(ss>>method>>sp>>tp>>label)||(ss>>extra))throw std::runtime_error("malformed job");
    const bool reference=method.find("reference5")==0,multi=(method=="multi_refined"||method=="multi_v2_refined"||method=="multi_v3_refined"),refine=method.find("refined")!=std::string::npos;
    if(!reference&&method!="eth10_raw"&&method.find("eth10_refined")!=0&&!multi)throw std::runtime_error("unknown policy");
    Timer online;const auto skp=trees(sp),tkp=trees(tp);CnParams p;p.threshold=reference?.05:.10;
    Timer dt;const auto sd=buildCnDescriptors(skp,p),td=buildCnDescriptors(tkp,p);const double descriptor=dt.ms();
    Timer mt;const auto match=matchCn(sd,td,skp,tkp,p);CnRecoveryParams rp;if(!reference){rp.mode=CnRecoveryMode::EvidenceConsensus;rp.trigger_matches=6;}const auto recovered=recoverCn(match,skp,tkp,p,rp);const double matching=mt.ms();
    std::vector<Eigen::Vector3d> a,b;for(const auto& [i,j]:recovered.pairs){a.push_back(skp[i]);b.push_back(tkp[j]);}
    Timer st;const auto initial=reference?kabsch(a,b):kabschRansac(a,b,RansacParams{}).T;const double solve=st.ms();
    std::vector<Proposal> poses;const auto pool=evidence(match);if(multi&&match.pairs.size()<6)poses=proposals(match,skp,tkp,pool);
    if(initial)poses.insert(poses.begin(),{*initial,globalSupport(*initial,skp,tkp),0});
    Timer rt;std::optional<RigidTransform> selected=initial;GeometryScore selected_score;int iterations=0;std::size_t refined_count=0;
    if(refine&&!poses.empty()) {
      std::vector<std::pair<double,std::size_t>> ranked;
      for(std::size_t k=0;k<poses.size();++k){const auto g=geometryScore(source.front(),target.front(),poses[k].pose);ranked.push_back({g.overlap/(.1+g.rmse)*std::log1p(poses[k].support),k});}
      std::stable_sort(ranked.begin(),ranked.end(),[](const auto&a,const auto&b){return a.first>b.first;});
      // Always retain the existing estimate among the refinement candidates.
      if(initial){ranked.erase(std::remove_if(ranked.begin(),ranked.end(),[](const auto& r){return r.second==0;}),ranked.end());ranked.insert(ranked.begin(),{0.,0});}
      double best=-1;for(std::size_t k=0;k<std::min<std::size_t>(multi?8:1,ranked.size());++k) {
        const auto refined=refineGeometry(source,target,poses[ranked[k].second].pose);++refined_count;
        const double value=refined.after.overlap/(.03+refined.after.rmse)*std::log1p(globalSupport(refined.pose,skp,tkp));
        if(value>best){best=value;selected=refined.pose;selected_score=refined.after;iterations=refined.iterations;}
      }
    }
    const double refine_ms=rt.ms();out<<label<<','<<method<<','<<bool(selected)<<','<<descriptor<<','<<matching<<','<<solve<<','<<refine_ms<<','<<online.ms()<<','<<poses.size()<<','<<refined_count<<','<<selected_score.overlap<<','<<selected_score.rmse<<','<<iterations;
    for(int i=0;i<3;++i)for(int j=0;j<3;++j)out<<','<<(selected?selected->R(i,j):std::nan(""));for(int i=0;i<3;++i)out<<','<<(selected?selected->t[i]:std::nan(""));out<<'\n';out.flush();
    std::cout<<"completed "<<label<<' '<<method<<std::endl;
  }
}
}
int main(int argc,char** argv) {
  try {
    const auto a=app::parseArgs(argc,argv,{"input","prepare","src_cache","tgt_cache","batch","out","help"});
    if(a.opts.count("input")&&a.opts.count("prepare")){prepare(a.opts.at("input"),a.opts.at("prepare"));return 0;}
    for(const auto* key:{"src_cache","tgt_cache","batch","out"})if(!a.opts.count(key)){std::cout<<"usage: cm_geometry_register --input=PLY --prepare=CACHE OR --src_cache=A --tgt_cache=B --batch=JOBS --out=CSV\n";return a.opts.count("help")?0:2;}
    const auto source=load(a.opts.at("src_cache")),target=load(a.opts.at("tgt_cache"));infer(source,target,a.opts.at("batch"),a.opts.at("out"));return 0;
  }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
