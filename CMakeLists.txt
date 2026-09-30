cmake_minimum_required(VERSION 3.16)
project(CircularMatchKeypoints CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Ubuntu's PCL (io) pulls in VTK 9.1, whose CMake config links VTK::mpi to
# MPI::MPI_C. Nothing here uses MPI; if it is not installed, provide an empty
# target so configuration does not fail.
find_package(MPI QUIET COMPONENTS C)
if(NOT TARGET MPI::MPI_C)
  add_library(MPI::MPI_C INTERFACE IMPORTED)
endif()

find_package(PCL 1.10 REQUIRED COMPONENTS common io octree kdtree search sample_consensus segmentation)
find_package(Eigen3 REQUIRED)
find_package(OpenMP)

add_library(cm_core
  src/config.cpp
  src/io.cpp
  src/las_reader.cpp
  src/sanity.cpp
  src/dtm.cpp
  src/voxel.cpp
  src/normals.cpp
  src/retention.cpp
  src/budget.cpp
  src/cluster.cpp
  src/cylinder.cpp
  src/tree_position.cpp
  src/pipeline.cpp
  src/matching.cpp
  src/experiment.cpp
  src/synth.cpp
)
target_include_directories(cm_core PUBLIC include ${PCL_INCLUDE_DIRS})
target_compile_definitions(cm_core PUBLIC ${PCL_DEFINITIONS})
target_link_libraries(cm_core PUBLIC ${PCL_LIBRARIES} Eigen3::Eigen)
if(OpenMP_CXX_FOUND)
  target_link_libraries(cm_core PUBLIC OpenMP::OpenMP_CXX)
endif()
target_compile_options(cm_core PRIVATE -Wall -Wextra)

foreach(app cm_extract cm_compare cm_synth cm_inspect)
  add_executable(${app} apps/${app}.cpp apps/app_common.cpp)
  target_link_libraries(${app} PRIVATE cm_core)
endforeach()

option(CM_BUILD_TESTS "Build tests" ON)
if(CM_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_executable(cm_tests
    tests/test_retention.cpp
    tests/test_crn.cpp
    tests/test_budget.cpp
    tests/test_io.cpp
    tests/test_sanity.cpp
    tests/test_dtm.cpp
    tests/test_tree_position.cpp
    tests/test_cylinder_seed.cpp
    tests/test_matching.cpp
    tests/test_synthetic.cpp
    tests/test_reproducibility.cpp
  )
  target_link_libraries(cm_tests PRIVATE cm_core GTest::gtest GTest::gtest_main)
  include(GoogleTest)
  gtest_discover_tests(cm_tests DISCOVERY_TIMEOUT 60)
endif()
