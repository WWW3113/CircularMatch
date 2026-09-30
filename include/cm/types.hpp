#pragma once
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <chrono>

namespace cm {

using PointT = pcl::PointXYZ;
using Cloud = pcl::PointCloud<PointT>;
using NormalT = pcl::Normal;
using NormalCloud = pcl::PointCloud<NormalT>;

class Timer {
 public:
  Timer() : t0_(std::chrono::steady_clock::now()) {}
  double ms() const {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0_).count();
  }

 private:
  std::chrono::steady_clock::time_point t0_;
};

}  // namespace cm
