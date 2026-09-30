#include <gtest/gtest.h>

#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>

#include <filesystem>
#include <fstream>

#include "cm/io.hpp"
#include "cm/synth.hpp"
#include "cm/types.hpp"

using namespace cm;
namespace fs = std::filesystem;

namespace {
std::string tmp(const std::string& name) {
  const auto dir = fs::temp_directory_path() / "cm_test_io";
  fs::create_directories(dir);
  return (dir / name).string();
}
std::vector<Eigen::Vector3d> samplePts() {
  return {{1.25, -2.5, 0.125}, {10.0, 20.0, 3.0}, {-7.125, 0.5, -1.75}, {0.001, 0.002, 0.003}};
}
void expectSame(const std::vector<Eigen::Vector3d>& a, const std::vector<Eigen::Vector3d>& b, double tol) {
  ASSERT_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) EXPECT_LE((a[i] - b[i]).cwiseAbs().maxCoeff(), tol) << i;
}
}  // namespace

TEST(IO, AsciiWithHeaderCommasAndExtraColumns) {
  const auto p = tmp("a.txt");
  {
    std::ofstream o(p);
    o << "# comment\nx,y,z,intensity\n1.25,-2.5,0.125,7\n10 20 3 1 2\n\n-7.125;0.5;-1.75\n0.001\t0.002\t0.003\nbad line\n";
  }
  const auto rc = readPointCloud(p);
  expectSame(rc.points, samplePts(), 1e-12);
  EXPECT_EQ(rc.skipped, 3u);  // comment, header, "bad line"
}

TEST(IO, PcdAsciiAndBinary) {
  Cloud c;
  for (const auto& q : samplePts()) c.push_back(PointT(float(q.x()), float(q.y()), float(q.z())));
  for (int mode = 0; mode < 3; ++mode) {
    const auto p = tmp("a" + std::to_string(mode) + ".pcd");
    if (mode == 0) pcl::io::savePCDFileASCII(p, c);
    if (mode == 1) pcl::io::savePCDFileBinary(p, c);
    if (mode == 2) pcl::io::savePCDFileBinaryCompressed(p, c);
    expectSame(readPointCloud(p).points, samplePts(), 1e-6);
  }
}

TEST(IO, PcdDoubleFieldsKeepPrecision) {
  const auto p = tmp("d.pcd");
  {
    std::ofstream o(p);
    o << "VERSION .7\nFIELDS x y z\nSIZE 8 8 8\nTYPE F F F\nCOUNT 1 1 1\nWIDTH 1\nHEIGHT 1\n"
         "VIEWPOINT 0 0 0 1 0 0 0\nPOINTS 1\nDATA ascii\n500123.4567 4400123.1234 101.2345\n";
  }
  const auto rc = readPointCloud(p);
  ASSERT_EQ(rc.points.size(), 1u);
  EXPECT_NEAR(rc.points[0].x(), 500123.4567, 1e-6);
  EXPECT_NEAR(rc.points[0].y(), 4400123.1234, 1e-6);
}

TEST(IO, PlyAsciiAndBinary) {
  Cloud c;
  for (const auto& q : samplePts()) c.push_back(PointT(float(q.x()), float(q.y()), float(q.z())));
  pcl::PLYWriter w;
  for (bool binary : {false, true}) {
    const auto p = tmp(binary ? "b.ply" : "a.ply");
    ASSERT_EQ(w.write(p, c, binary), 0);
    expectSame(readPointCloud(p).points, samplePts(), 1e-6);
  }
}

TEST(IO, LasAllVersionsAndFormats) {
  const Eigen::Vector3d off(500000.0, 4400000.0, 100.0);
  std::vector<Eigen::Vector3d> pts;
  for (const auto& q : samplePts()) pts.push_back(q + off);
  const int maxFormat[5] = {1, 1, 3, 5, 10};
  for (int minor = 0; minor <= 4; ++minor)
    for (int f = 0; f <= maxFormat[minor]; ++f)
      for (std::uint16_t extra : {std::uint16_t(0), std::uint16_t(6)}) {
        const auto p = tmp("v" + std::to_string(minor) + "f" + std::to_string(f) + "e" + std::to_string(extra) + ".las");
        writeLas(p, pts, minor, f, 0.001, off, -1, extra);
        const auto rc = readPointCloud(p);
        SCOPED_TRACE(rc.detail);
        expectSame(rc.points, pts, 0.0005 + 1e-9);
      }
}

TEST(IO, LasRejectsUnsupported) {
  const auto pts = samplePts();
  const auto p = tmp("bad.las");
  writeLas(p, pts, 2, 6);  // format 6 needs LAS 1.4
  EXPECT_THROW(readPointCloud(p), std::runtime_error);
  writeLas(p, pts, 4, 11, 0.001, Eigen::Vector3d::Zero(), 11);  // format 11 does not exist
  EXPECT_THROW(readPointCloud(p), std::runtime_error);
  writeLas(p, pts, 2, 3, 0.001, Eigen::Vector3d::Zero(), 3 | 0x80);  // compressed bit (LAZ)
  EXPECT_THROW(readPointCloud(p), std::runtime_error);
  writeLas(p, pts, 2, 3);
  fs::resize_file(p, fs::file_size(p) - 5);  // truncated
  EXPECT_THROW(readPointCloud(p), std::runtime_error);
  {
    std::ofstream o(p, std::ios::binary);
    o << std::string(300, 'x');
  }
  EXPECT_THROW(readPointCloud(p), std::runtime_error);  // no LASF signature
}

TEST(IO, LasRecordLengthTooShortRejected) {
  const auto p = tmp("short.las");
  writeLas(p, samplePts(), 2, 3);
  // Patch record length (offset 105) to 30 < 34 (format 3 minimum).
  std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
  f.seekp(105);
  const std::uint16_t len = 30;
  f.write(reinterpret_cast<const char*>(&len), 2);
  f.close();
  EXPECT_THROW(readPointCloud(p), std::runtime_error);
}

TEST(IO, LazRejectedWithHint) {
  const auto p = tmp("x.laz");
  { std::ofstream o(p); o << "x"; }
  try {
    readPointCloud(p);
    FAIL() << "expected throw";
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("Decompress"), std::string::npos);
  }
}

TEST(IO, UnknownExtensionRejected) {
  const auto p = tmp("x.e57");
  { std::ofstream o(p); o << "x"; }
  EXPECT_THROW(readPointCloud(p), std::runtime_error);
}
