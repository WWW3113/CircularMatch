#pragma once
#include <Eigen/Core>
#include <vector>
#include "cm/registration.hpp"
namespace cm {
struct SurfacePoint { Eigen::Vector3d p,n; };
struct SurfaceLevel { double voxel=0; std::vector<SurfacePoint> points; };
struct GeometryScore { double overlap=0, rmse=1e9; std::size_t support=0; };
struct RefineResult { RigidTransform pose; int iterations=0; bool solved=false; GeometryScore before,after; };
GeometryScore geometryScore(const SurfaceLevel& source,const SurfaceLevel& target,const RigidTransform& pose);
RefineResult refineGeometry(const std::vector<SurfaceLevel>& source,const std::vector<SurfaceLevel>& target,const RigidTransform& initial);
}
