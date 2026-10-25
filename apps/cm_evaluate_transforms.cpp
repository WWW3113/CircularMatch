// Evaluation-only executable. Kept separate from all GT-free inference code.
#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "app_common.hpp"
#include "cm/io.hpp"
#include "cm/registration.hpp"
using namespace cm;
namespace {
std::vector<std::string> split(std::string line) {
  if(!line.empty()&&line.back()=='\r')line.pop_back();std::stringstream ss(line);std::vector<std::string> cells;std::string x;while(std::getline(ss,x,','))cells.push_back(x);return cells;
}
}
int main(int argc,char** argv) {
  try {
    auto a=app::parseArgs(argc,argv,{"src_cloud","gt","estimates","out","help"});
    for(const auto* key:{"src_cloud","gt","estimates","out"})if(!a.opts.count(key)){std::cout<<"usage: cm_evaluate_transforms --src_cloud=PLY --gt=MATRIX --estimates=CSV --out=CSV\n";return a.opts.count("help")?0:2;}
    std::ifstream gtfile(a.opts.at("gt"));double gt[16];for(double& x:gt)if(!(gtfile>>x))throw std::runtime_error("invalid GT matrix");
    Eigen::Matrix3d gr;Eigen::Vector3d gt_t;for(int i=0;i<3;++i){gt_t[i]=gt[4*i+3];for(int j=0;j<3;++j)gr(i,j)=gt[4*i+j];}
    const auto cloud=readPointCloud(a.opts.at("src_cloud"));if(cloud.skipped||cloud.points.empty())throw std::runtime_error("nonfinite or empty source input");
    std::ifstream input(a.opts.at("estimates"));std::string line;if(!std::getline(input,line))throw std::runtime_error("missing estimates");const auto header=split(line);
    auto col=[&](const std::string& key){auto it=std::find(header.begin(),header.end(),key);if(it==header.end())throw std::runtime_error("missing estimate column");return std::size_t(it-header.begin());};
    std::vector<std::vector<std::string>> rows;while(std::getline(input,line))if(!line.empty()){rows.push_back(split(line));if(rows.back().size()!=header.size())throw std::runtime_error("malformed estimates");}
    struct Result {bool valid=false,success=false;double rot=NAN,trans=NAN,ep=NAN;};std::vector<Result> results(rows.size());
#pragma omp parallel for schedule(dynamic,1)
    for(std::size_t k=0;k<rows.size();++k) {
      if(rows[k][col("pose_ok")]!="1")continue;
      RigidTransform estimate;for(int i=0;i<3;++i){estimate.t[i]=std::stod(rows[k][col("t"+std::to_string(i))]);for(int j=0;j<3;++j)estimate.R(i,j)=std::stod(rows[k][col("r"+std::to_string(i)+std::to_string(j))]);}
      auto& result=results[k];if(!estimate.R.allFinite()||!estimate.t.allFinite()||(estimate.R.transpose()*estimate.R-Eigen::Matrix3d::Identity()).norm()>1e-5)continue;
      const Eigen::Matrix3d d=estimate.R-gr;const Eigen::Vector3d t=estimate.t-gt_t;double sum=0;
#pragma omp simd reduction(+:sum)
      for(std::size_t j=0;j<cloud.points.size();++j) {
        const auto& p=cloud.points[j];
        const double x=d(0,0)*p.x()+d(0,1)*p.y()+d(0,2)*p.z()+t.x();
        const double y=d(1,0)*p.x()+d(1,1)*p.y()+d(1,2)*p.z()+t.y();
        const double z=d(2,0)*p.x()+d(2,1)*p.y()+d(2,2)*p.z()+t.z();sum+=std::sqrt(x*x+y*y+z*z);
      }
      result.valid=true;result.ep=sum/cloud.points.size();result.rot=rotationErrorDeg(estimate.R,gr);result.trans=t.norm();result.success=result.rot<2&&result.trans<.5;
    }
    std::ofstream out(a.opts.at("out"));out<<std::setprecision(17);for(std::size_t i=0;i<header.size();++i)out<<(i?",":"")<<header[i];out<<",status,rot_err_deg,trans_err_m,e_p_m,success,source_points\n";
    for(std::size_t k=0;k<rows.size();++k){for(std::size_t i=0;i<header.size();++i)out<<(i?",":"")<<rows[k][i];const auto& r=results[k];out<<','<<(r.valid?"computed":"no_transform")<<','<<r.rot<<','<<r.trans<<','<<r.ep<<','<<r.success<<','<<cloud.points.size()<<'\n';}
    if(!out)throw std::runtime_error("cannot write evaluation");return 0;
  }catch(const std::exception& e){std::cerr<<"ERROR: "<<e.what()<<'\n';return 1;}
}
