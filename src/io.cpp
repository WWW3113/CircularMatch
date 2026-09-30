#include "cm/io.hpp"

#include <pcl/PCLPointCloud2.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/ply_io.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cm {

namespace {

std::string lowerExt(const std::string& path) {
  const auto dot = path.find_last_of('.');
  const auto slash = path.find_last_of("/\\");
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
  std::string e = path.substr(dot + 1);
  std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return std::tolower(c); });
  return e;
}

// Parses up to `want` leading numbers from a line; separators: space, tab, comma, semicolon.
int parseNumbers(const std::string& line, double* out, int want) {
  int n = 0;
  const char* p = line.c_str();
  while (*p && n < want) {
    while (*p == ' ' || *p == '\t' || *p == ',' || *p == ';' || *p == '\r') ++p;
    if (!*p) break;
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    if (end == p) return n;  // non-numeric token
    if (*end && *end != ' ' && *end != '\t' && *end != ',' && *end != ';' && *end != '\r' && *end != '\n')
      return n;  // e.g. "12abc"
    out[n++] = v;
    p = end;
  }
  return n;
}

RawCloud readAscii(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open " + path);
  RawCloud rc;
  rc.format = "xyz";
  std::string line;
  double v[3];
  while (std::getline(in, line)) {
    const auto first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos) continue;
    if (line[first] == '#' || line[first] == '/') { ++rc.skipped; continue; }
    if (parseNumbers(line, v, 3) < 3 || !std::isfinite(v[0]) || !std::isfinite(v[1]) || !std::isfinite(v[2])) {
      ++rc.skipped;
      continue;
    }
    rc.points.emplace_back(v[0], v[1], v[2]);
  }
  if (rc.points.empty()) throw std::runtime_error(path + ": no x y z rows found");
  return rc;
}

double readField(const std::uint8_t* p, const pcl::PCLPointField& f) {
  switch (f.datatype) {
    case pcl::PCLPointField::FLOAT32: { float v; std::memcpy(&v, p, 4); return v; }
    case pcl::PCLPointField::FLOAT64: { double v; std::memcpy(&v, p, 8); return v; }
    case pcl::PCLPointField::INT8: { std::int8_t v; std::memcpy(&v, p, 1); return v; }
    case pcl::PCLPointField::UINT8: { std::uint8_t v; std::memcpy(&v, p, 1); return v; }
    case pcl::PCLPointField::INT16: { std::int16_t v; std::memcpy(&v, p, 2); return v; }
    case pcl::PCLPointField::UINT16: { std::uint16_t v; std::memcpy(&v, p, 2); return v; }
    case pcl::PCLPointField::INT32: { std::int32_t v; std::memcpy(&v, p, 4); return v; }
    case pcl::PCLPointField::UINT32: { std::uint32_t v; std::memcpy(&v, p, 4); return v; }
    default: throw std::runtime_error("unsupported field datatype for '" + f.name + "'");
  }
}

RawCloud fromCloud2(const pcl::PCLPointCloud2& c, const std::string& fmt, const std::string& path) {
  const pcl::PCLPointField* fx = nullptr;
  const pcl::PCLPointField* fy = nullptr;
  const pcl::PCLPointField* fz = nullptr;
  for (const auto& f : c.fields) {
    if (f.name == "x") fx = &f;
    if (f.name == "y") fy = &f;
    if (f.name == "z") fz = &f;
  }
  if (!fx || !fy || !fz) throw std::runtime_error(path + ": missing x/y/z fields");
  RawCloud rc;
  rc.format = fmt;
  const std::size_t n = static_cast<std::size_t>(c.width) * c.height;
  rc.points.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t row = c.width ? i / c.width : 0, col = c.width ? i % c.width : 0;
    const std::uint8_t* base = c.data.data() + row * c.row_step + col * c.point_step;
    const double x = readField(base + fx->offset, *fx);
    const double y = readField(base + fy->offset, *fy);
    const double z = readField(base + fz->offset, *fz);
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { ++rc.skipped; continue; }
    rc.points.emplace_back(x, y, z);
  }
  if (rc.points.empty()) throw std::runtime_error(path + ": no finite points");
  auto typeName = [](std::uint8_t t) { return t == pcl::PCLPointField::FLOAT64 ? "float64" : "float32/int"; };
  rc.detail = std::string("xyz field type ") + typeName(fx->datatype);
  return rc;
}

}  // namespace

RawCloud readPointCloud(const std::string& path) {
  const std::string ext = lowerExt(path);
  if (ext == "laz")
    throw std::runtime_error(path + ": LAZ (compressed LAS) is not supported. Decompress first, e.g. "
                             "`laszip -i in.laz -o out.las` or `pdal translate in.laz out.las`.");
  if (ext == "xyz" || ext == "txt" || ext == "csv" || ext == "asc") return readAscii(path);
  if (ext == "las") return readLas(path);
  if (ext == "pcd") {
    pcl::PCLPointCloud2 c;
    if (pcl::io::loadPCDFile(path, c) < 0) throw std::runtime_error(path + ": PCL failed to read PCD");
    return fromCloud2(c, "pcd", path);
  }
  if (ext == "ply") {
    pcl::PCLPointCloud2 c;
    pcl::PLYReader r;
    if (r.read(path, c) < 0) throw std::runtime_error(path + ": PCL failed to read PLY");
    return fromCloud2(c, "ply", path);
  }
  throw std::runtime_error(path + ": unsupported extension '." + ext +
                           "' (supported: xyz txt csv asc pcd ply las)");
}

std::vector<Eigen::Vector3d> readReferencePositions(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open reference file " + path);
  std::vector<Eigen::Vector3d> out;
  std::string line;
  double v[3];
  while (std::getline(in, line)) {
    const int n = parseNumbers(line, v, 3);
    if (n >= 2) out.emplace_back(v[0], v[1], n >= 3 ? v[2] : std::nan(""));
  }
  if (out.empty()) throw std::runtime_error(path + ": no reference positions found");
  return out;
}

}  // namespace cm
