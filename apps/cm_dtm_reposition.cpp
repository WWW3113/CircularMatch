// Position-only DTM experiment. Reuses fitted axes; no GT input or refitting.
#include <Eigen/Core>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include "app_common.hpp"
#include "cm/dtm.hpp"
#include "cm/io.hpp"
#include "cm/sanity.hpp"
#include "cm/tree_position.hpp"
#include "cm/types.hpp"

using namespace cm;
namespace {
struct Csv {
  std::vector<std::string> header;
  std::vector<std::vector<std::string>> rows;
  std::size_t col(const std::string& key) const {
    for (std::size_t k=0;k<header.size();++k)if(header[k]==key)return k;
    throw std::runtime_error("missing CSV column "+key);
  }
};
std::vector<std::string> split(std::string line) {
  if(!line.empty() && line.back()=='\r')line.pop_back();
  std::vector<std::string> out;std::stringstream ss(line);std::string value;
  while(std::getline(ss,value,','))out.push_back(value);
  return out;
}
Csv read(const std::string& path) {
  std::ifstream in(path);if(!in)throw std::runtime_error("cannot read "+path);
  Csv csv;std::string line;if(!std::getline(in,line))throw std::runtime_error("empty CSV "+path);csv.header=split(line);
  while(std::getline(in,line))if(!line.empty()) {
    auto row=split(line);if(row.size()!=csv.header.size())throw std::runtime_error("malformed CSV "+path);
    csv.rows.push_back(std::move(row));
  }
  return csv;
}
void writeRow(std::ostream& out,const std::vector<std::string>& row) {
  for(std::size_t k=0;k<row.size();++k)out<<(k?",":"")<<row[k];out<<'\n';
}
Eigen::Vector3d vec(const Csv& csv,const std::vector<std::string>& row,const std::string& prefix) {
  Eigen::Vector3d v;for(int k=0;k<3;++k)v[k]=std::stod(row[csv.col(prefix+"xyz"[k])]);
  if(!v.allFinite())throw std::runtime_error("non-finite coordinate");return v;
}
struct Job {std::string trees,clusters;};
}
int main(int argc,char** argv) {
  try {
    auto a=app::parseArgs(argc,argv,{"input","batch","out","profiles","help"});
    if(!a.opts.count("input")||!a.opts.count("batch")||!a.opts.count("out")) {
      std::cout<<"usage: cm_dtm_reposition --input=PLY --batch=JOBS --out=DIR [--profiles=baseline,smooth,coarse,quantile1]\n";
      return a.opts.count("help")?0:2;
    }
    std::vector<Job> jobs;std::ifstream batch(a.opts.at("batch"));if(!batch)throw std::runtime_error("cannot read batch");
    std::string line;while(std::getline(batch,line))if(!line.empty()) {
      std::stringstream ss(line);Job j;std::string extra;
      if(!(ss>>j.trees>>j.clusters)||(ss>>extra))throw std::runtime_error("malformed batch row");jobs.push_back(j);
    }
    if(jobs.empty())throw std::runtime_error("empty batch");
    const auto profiles=split(a.opts.count("profiles")?a.opts.at("profiles"):"baseline,smooth,coarse,quantile1");
    const auto raw=readPointCloud(a.opts.at("input"));const auto loaded=prepareCloud(raw,a.cfg.scanner);
    std::cout<<formatReport(loaded.report,raw)<<std::endl;
    for(const auto& profile:profiles) {
      Config cfg=a.cfg;
      if(profile=="smooth"){cfg.dtm.fit_radius=6;cfg.dtm.fill_max_radius=6;}
      else if(profile=="coarse"){cfg.dtm.cell=1.;cfg.dtm.fit_radius=3;cfg.dtm.fill_max_radius=6;}
      else if(profile=="quantile1"){cfg.dtm.ground_select="percentile";cfg.dtm.percentile=1.;}
      else if(profile!="baseline")throw std::runtime_error("unknown profile "+profile);
      const auto dest=a.opts.at("out")+"/"+profile;app::ensureDir(dest);app::writeConfigDump(cfg,dest+"/effective_config.ini");
      Timer timer;const Dtm dtm=Dtm::build(*loaded.cloud,cfg.dtm);const auto& st=dtm.stats();
      std::ofstream stats(dest+"/dtm_stats.csv");stats<<"input_points,nx,ny,cells_with_sample,rejected_slope,rejected_plane,cells_valid,cells_filled,build_ms\n"
        <<raw.points.size()<<','<<st.nx<<','<<st.ny<<','<<st.cells_with_sample<<','<<st.samples_rejected_slope<<','<<st.samples_rejected<<','<<st.cells_valid<<','<<st.cells_filled<<','<<timer.ms()<<'\n';
      for(const auto& job:jobs) {
        const Csv trees=read(job.trees),clusters=read(job.clusters);
        std::map<int,const std::vector<std::string>*> axes;
        for(const auto& row:clusters.rows)if(!axes.emplace(std::stoi(row[clusters.col("cluster_id")]),&row).second)throw std::runtime_error("duplicate cluster id");
        const auto name=std::filesystem::path(job.trees).filename().string();
        std::ofstream out(dest+"/"+name),audit(dest+"/"+name+".audit.csv");if(!out||!audit)throw std::runtime_error("cannot write output");
        writeRow(out,trees.header);audit<<std::setprecision(12)<<"cluster_id,intersection_ok,old_x,old_y,old_z,new_x,new_y,new_z,old_dtm_residual_m\n";
        for(auto row:trees.rows) {
          const int id=std::stoi(row[trees.col("cluster_id")]);const auto found=axes.find(id);if(found==axes.end())throw std::runtime_error("tree missing axis");
          const auto& fit=*found->second;const Eigen::Vector3d old=vec(trees,row,"")-loaded.offset;
          const Eigen::Vector3d point=vec(clusters,fit,"axis_")-loaded.offset,dir=vec(clusters,fit,"dir_");
          if(dir.norm()<1e-9)throw std::runtime_error("zero axis direction");
          const auto position=axisDtmIntersection(point,dir,dtm,cfg.intersect,(old-point).dot(dir.normalized()));
          const auto height=dtm.height(old.x(),old.y());
          // Baseline preserves the original rounded CSV values exactly. Other
          // profiles drop failed intersections and expose every drop in audit.
          const Eigen::Vector3d updated=profile=="baseline"?old:(position?position->point:old);
          audit<<id<<','<<bool(position)<<','<<old.x()<<','<<old.y()<<','<<old.z()<<','<<updated.x()<<','<<updated.y()<<','<<updated.z()<<',';
          if(height)audit<<old.z()-*height;else audit<<"nan";audit<<'\n';
          if(profile!="baseline"&&!position)continue;
          if(profile!="baseline")for(int k=0;k<3;++k) {
            std::ostringstream value;value<<std::fixed<<std::setprecision(4)<<(updated[k]+loaded.offset[k]);row[trees.col(std::string(1,"xyz"[k]))]=value.str();
          }
          writeRow(out,row);
        }
      }
      std::cout<<"completed profile "<<profile<<std::endl;
    }
    return 0;
  }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
