#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "cm/config.hpp"
#include "cm/sanity.hpp"

using namespace cm;

namespace {
RawCloud cloudAt(const Eigen::Vector3d& off) {
  RawCloud r;
  for (double x = -10; x <= 10; x += 0.5)
    for (double y = -10; y <= 10; y += 0.5) r.points.push_back(off + Eigen::Vector3d(x, y, 0.1 * x));
  return r;
}
}  // namespace

TEST(Sanity, LargeCoordinatesWithDefaultScannerStop) {
  Config cfg;
  const RawCloud r = cloudAt({500000, 4400000, 100});
  try {
    prepareCloud(r, cfg.scanner);
    FAIL() << "expected stop";
  } catch (const std::runtime_error& e) {
    EXPECT_NE(std::string(e.what()).find("scanner.x0"), std::string::npos) << e.what();
  }
}

TEST(Sanity, LargeCoordinatesWithScannerPositionOk) {
  Config cfg;
  cfg.set("scanner.x0", "500000");
  cfg.set("scanner.y0", "4400000");
  cfg.set("scanner.z0", "100");
  const auto lc = prepareCloud(cloudAt({500000, 4400000, 100}), cfg.scanner);
  EXPECT_TRUE(lc.report.warnings.empty());
  // local coordinates are small and exactly representable here
  EXPECT_FLOAT_EQ((*lc.cloud)[0].x, -10.f);
  EXPECT_DOUBLE_EQ(lc.offset.x(), 500000);
}

TEST(Sanity, WrongScannerPositionStops) {
  Config cfg;
  cfg.set("scanner.x0", "0");  // explicitly given, but wrong
  EXPECT_THROW(prepareCloud(cloudAt({500000, 4400000, 100}), cfg.scanner), std::runtime_error);
}

TEST(Sanity, FarNearestPointWarns) {
  Config cfg;
  const auto lc = prepareCloud(cloudAt({30, 0, 0}), cfg.scanner);  // nearest point 20 m away
  ASSERT_FALSE(lc.report.warnings.empty());
  EXPECT_NEAR(lc.report.nearest_horizontal, 20.0, 1e-9);
}

TEST(Sanity, ConfigTracksExplicitScanner) {
  Config a;
  EXPECT_FALSE(a.scanner.position_given);
  a.applyArgs({"--scanner.z0=1.5"});
  EXPECT_TRUE(a.scanner.position_given);
  Config b = a;  // copy keeps values and a working registry
  b.set("dtm.cell", "0.75");
  EXPECT_DOUBLE_EQ(b.dtm.cell, 0.75);
  EXPECT_DOUBLE_EQ(a.dtm.cell, 0.5);
  EXPECT_THROW(a.set("no.such", "1"), std::runtime_error);
  EXPECT_THROW(a.set("dtm.cell", "abc"), std::runtime_error);
}

TEST(Sanity, ConfigDumpRoundTripKeepsScannerUnset) {
  const auto dir = std::filesystem::temp_directory_path() / "cm_test_cfg";
  std::filesystem::create_directories(dir);
  const std::string path = (dir / "c.ini").string();
  Config a;
  a.set("dtm.cell", "0.75");
  {
    std::ofstream o(path);
    a.dump(o);
  }
  Config b;
  b.loadIni(path);
  EXPECT_FALSE(b.scanner.position_given);
  EXPECT_DOUBLE_EQ(b.dtm.cell, 0.75);
  a.set("scanner.x0", "12.5");
  {
    std::ofstream o(path);
    a.dump(o);
  }
  Config c;
  c.loadIni(path);
  EXPECT_TRUE(c.scanner.position_given);
  EXPECT_DOUBLE_EQ(c.scanner.x0, 12.5);
}
