// Export retained stem candidates and reproducible Euclidean-cluster membership.
// Diagnostic only: the existing extraction and matching algorithms are unchanged.
#include <fstream>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include "app_common.hpp"
#include "cm/cluster.hpp"
#include "cm/experiment.hpp"
#include "cm/pipeline.hpp"

using namespace cm;
int main(int argc,char** argv) {
  try {
    auto a=app::parseArgs(argc,argv,{"input","out","help"});
    if (!a.opts.count("input") || !a.opts.count("out")) {
      std::cout<<"usage: cm_candidate_export --input=PLY --out=DIR [--experiment.seed_base=21 --experiment.n_seeds=1]\n";
      return a.opts.count("help") ? 0 : 2;
    }
    const Config& cfg=a.cfg;const auto out=a.opts.at("out");app::ensureDir(out);
    if(cfg.experiment.n_seeds<=0 || std::uint64_t(cfg.experiment.n_seeds-1)>
       std::numeric_limits<std::uint64_t>::max()-cfg.experiment.seed_base)
      throw std::runtime_error("seed range must be nonempty and must not overflow uint64");
    const auto raw=readPointCloud(a.opts.at("input"));const auto loaded=prepareCloud(raw,cfg.scanner);
    const auto pre=preprocess(loaded,cfg);
    std::ofstream points(out+"/vertical.csv");if(!points)throw std::runtime_error("cannot write points");
    points<<std::setprecision(10)<<"index,x,y,z,nx,ny,nz,ground_z\n";
    std::vector<int> inverse(pre.voxel->size(),-1);
    for(std::size_t k=0;k<pre.vertical.size();++k) {
      const int i=pre.vertical[k];inverse[i]=int(k);
      const Eigen::Vector3d p=(*pre.voxel)[i].getVector3fMap().cast<double>();const auto& n=(*pre.normals)[i];
      const auto ground=pre.dtm.height(p.x(),p.y());
      points<<k<<','<<p.x()+pre.offset.x()<<','<<p.y()+pre.offset.y()<<','<<p.z()+pre.offset.z()<<','
            <<n.normal_x<<','<<n.normal_y<<','<<n.normal_z<<','<<(ground ? *ground+pre.offset.z() : std::nan(""))<<'\n';
    }
    const auto model=RetentionModel::make(parseRetentionKind(cfg.retention.version),cfg.retention);
    for(int s=0;s<cfg.experiment.n_seeds;++s) {
      const auto seed=cfg.experiment.seed_base+std::uint64_t(s);const auto u=computeUniforms(pre,seed,cfg);
      std::vector<int> kept;
      for(std::size_t k=0;k<pre.vertical.size();++k)if(u[k]<model(pre.dist[k]))kept.push_back(pre.vertical[k]);
      const auto clusters=euclideanClusters(*pre.voxel,kept,cfg.cluster);
      std::vector<int> labels(pre.vertical.size(),-1);
      for(std::size_t c=0;c<clusters.size();++c)for(int i:clusters[c])labels[inverse[i]]=int(c);
      std::ofstream f(out+"/seed"+std::to_string(seed)+"_members.csv");if(!f)throw std::runtime_error("cannot write membership");
      f<<"index,cluster_id\n";
      for(int i:kept)f<<inverse[i]<<','<<labels[inverse[i]]<<'\n';
      std::cout<<"seed "<<seed<<": "<<kept.size()<<" points, "<<clusters.size()<<" clusters\n";
    }
    app::writeConfigDump(cfg,out+"/effective_config.ini");
    return 0;
  }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
