// Optional density-gated arc audit/re-evaluation; original cm_core stays intact.
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <pcl/search/kdtree.h>
#include "app_common.hpp"
#include "cm/cluster.hpp"
#include "cm/experiment.hpp"
#include "cm/pipeline.hpp"
#include "cm/sparse_arc.hpp"
#include "cm/tree_position.hpp"

using namespace cm;
int main(int argc,char** argv) {
  try {
    auto a=app::parseArgs(argc,argv,{"input","out","arc_mode","sparse_normal_weight","sparse_dist_threshold","sparse_min_arc","sparse_min_normal_ratio","help"});
    if(!a.opts.count("input") || !a.opts.count("out")) {
      std::cout<<"usage: cm_sparse_extract --input=PLY --out=DIR --arc_mode=auto|occupied|trimmed90 [config overrides]\n";
      return a.opts.count("help")?0:2;
    }
    const auto mode=a.opts.count("arc_mode")?a.opts.at("arc_mode"):"occupied";
    if(mode!="occupied" && mode!="trimmed90" && mode!="auto")throw std::runtime_error("unknown arc_mode");
    Config cfg=a.cfg;
    if(cfg.experiment.n_seeds<=0 || std::uint64_t(cfg.experiment.n_seeds-1)>
       std::numeric_limits<std::uint64_t>::max()-cfg.experiment.seed_base)
      throw std::runtime_error("invalid seed range");
    const auto out=a.opts.at("out");app::ensureDir(out);
    const auto raw=readPointCloud(a.opts.at("input"));const auto loaded=prepareCloud(raw,cfg.scanner);
    const auto pre=preprocess(loaded,cfg);
    std::cout<<formatReport(loaded.report,raw)<<'\n';
    double spacing=0;
    const bool overrides=a.opts.count("sparse_normal_weight") || a.opts.count("sparse_dist_threshold") ||
                         a.opts.count("sparse_min_arc") || a.opts.count("sparse_min_normal_ratio");
    if((mode=="auto" || overrides) && pre.voxel->size()>=2) {
      pcl::search::KdTree<PointT> tree;tree.setInputCloud(pre.voxel);
      std::vector<double> nn;pcl::Indices ix;std::vector<float> dd;
      for(std::size_t i=0;i<pre.voxel->size();i+=std::max<std::size_t>(1,pre.voxel->size()/2048)) {
        if(tree.nearestKSearch((*pre.voxel)[i],2,ix,dd)==2)nn.push_back(std::sqrt(dd[1]));
      }
      if(!nn.empty()){std::sort(nn.begin(),nn.end());spacing=nn[nn.size()/2];}
    }
    const bool low_density=spacing>0.05;
    const bool sparse=mode=="trimmed90" || (mode=="auto" && low_density);
    if(low_density) {
      if(a.opts.count("sparse_normal_weight"))cfg.cylinder.normal_weight=std::stod(a.opts.at("sparse_normal_weight"));
      if(a.opts.count("sparse_dist_threshold"))cfg.cylinder.dist_threshold=std::stod(a.opts.at("sparse_dist_threshold"));
      if(a.opts.count("sparse_min_arc"))cfg.cylinder.min_arc_deg=std::stod(a.opts.at("sparse_min_arc"));
      if(a.opts.count("sparse_min_normal_ratio"))cfg.cylinder.min_normal_ratio=std::stod(a.opts.at("sparse_min_normal_ratio"));
    }
    std::ofstream meta(out+"/arc_policy.json");
    meta<<std::setprecision(12)<<"{\"mode\":\""<<mode<<"\",\"median_nearest_m\":"<<spacing
        <<",\"sparse_span_used\":"<<(sparse?"true":"false")<<",\"density_threshold_m\":0.05,\"support_fraction\":0.90}\n";
    app::writeConfigDump(cfg,out+"/effective_config.ini");
    const auto model=RetentionModel::make(parseRetentionKind(cfg.retention.version),cfg.retention);
    for(int k=0;k<cfg.experiment.n_seeds;++k) {
      const auto seed=cfg.experiment.seed_base+std::uint64_t(k);
      std::vector<char> flags;auto r=runStages(pre,model,seed,cfg,nullptr,&flags);
      if(sparse) {
        Timer timer;std::vector<int> kept;
        for(std::size_t i=0;i<flags.size();++i)if(flags[i])kept.push_back(pre.vertical[i]);
        const auto clusters=euclideanClusters(*pre.voxel,kept,cfg.cluster);
        r.trees.clear();r.fail_counts.fill(0);
        std::ofstream audit(out+"/arc_seed"+std::to_string(seed)+".csv");
        audit<<"cluster_id,occupied_deg,trimmed90_deg,old_fail_mask,new_fail_mask,n_inliers\n";
        for(std::size_t c=0;c<r.diag.size();++c) {
          auto& dg=r.diag[c];auto& f=dg.fit;const auto old_mask=f.fail_mask;const auto occupied=f.arc_deg;
          dg.has_position=false;
          if(!(f.fail_mask&((1u<<int(FitFail::TooFewPoints))|(1u<<int(FitFail::NoModel))))) {
            const auto e1=f.axis_dir.unitOrthogonal();const Eigen::Vector3d e2=f.axis_dir.cross(e1);
            std::vector<double> angles;
            for(int ix:clusters[c]) {
              const Eigen::Vector3d w=(*pre.voxel)[ix].getVector3fMap().cast<double>()-f.axis_point;
              const Eigen::Vector3d radial=w-w.dot(f.axis_dir)*f.axis_dir;
              if(std::abs(radial.norm()-f.radius)<=cfg.cylinder.dist_threshold)
                angles.push_back(std::atan2(radial.dot(e2),radial.dot(e1)));
            }
            f.arc_deg=trimmedArcDegrees(angles);
            f.fail_mask&=~((1u<<int(FitFail::ArcCoverageLow))|(1u<<int(FitFail::NoDtmIntersection)));
            if(cfg.cylinder.check_arc_coverage && (angles.size()<15 || f.arc_deg<cfg.cylinder.min_arc_deg))
              f.fail_mask|=1u<<int(FitFail::ArcCoverageLow);
          }
          f.fail=FitFail::None;
          for(int reason=1;reason<int(FitFail::Count);++reason)if(f.fail_mask&(1u<<reason)){f.fail=FitFail(reason);break;}
          if(f.fail==FitFail::None) {
            const double ref=(f.centroid-f.axis_point).dot(f.axis_dir);
            const auto pos=axisDtmIntersection(f.axis_point,f.axis_dir,pre.dtm,cfg.intersect,ref);
            if(pos){dg.has_position=true;r.trees.push_back({int(c),pos->point,f.radius,f.tilt_deg,f.n_points,f.n_inliers});}
            else{f.fail=FitFail::NoDtmIntersection;f.fail_mask|=1u<<int(f.fail);}
          }
          ++r.fail_counts[int(f.fail)];
          audit<<c<<','<<occupied<<','<<f.arc_deg<<','<<old_mask<<','<<f.fail_mask<<','<<f.n_inliers<<'\n';
        }
        r.stages[2].name="7 original_fit";r.stages[3].name="8 original_position";
        r.stages.push_back({"9 sparse_arc",r.trees.size(),timer.ms()});
      }
      const auto stem=out+"/step_seed"+std::to_string(seed);
      writeTreesCsv(stem+"_trees.csv",r,pre.offset);writeClustersCsv(stem+"_clusters.csv",r,pre.offset);
      writeRetentionBinsCsv(stem+"_retention_bins.csv",retentionBins(pre.dist,flags,model));
      printRun(std::cout,r,pre);std::cout<<"completed seed "<<seed<<std::endl;
    }
    return 0;
  }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
