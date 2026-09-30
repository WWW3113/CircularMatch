// Minimal uncompressed LAS reader: XYZ only.
// Public header layout (ASPRS LAS 1.0-1.4), little-endian:
//   0   char[4]  "LASF"
//   24  u8       version major, 25 u8 version minor
//   94  u16      header size
//   96  u32      offset to point data
//   104 u8       point data record format (bits 6/7 set => compressed, LAZ)
//   105 u16      point data record length
//   107 u32      legacy number of point records
//   131 f64[3]   scale x y z,  155 f64[3] offset x y z
//   247 u64      number of point records (LAS 1.4, header size >= 375)
// Every point format 0-10 starts with int32 X, Y, Z.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "cm/io.hpp"

namespace cm {

namespace {

static_assert(sizeof(double) == 8, "double must be 64-bit");

bool hostLittleEndian() {
  const std::uint16_t x = 1;
  std::uint8_t b;
  std::memcpy(&b, &x, 1);
  return b == 1;
}

template <typename T>
T get(const std::uint8_t* buf, std::size_t off) {
  T v;
  std::memcpy(&v, buf + off, sizeof(T));
  return v;
}

// Minimum record length for formats 0..10.
constexpr std::uint16_t kMinRecordLength[11] = {20, 28, 26, 34, 57, 63, 30, 36, 38, 59, 67};

// Highest point format allowed per LAS minor version (1.0 .. 1.4).
int maxFormatForMinor(int minor) {
  switch (minor) {
    case 0: case 1: return 1;
    case 2: return 3;
    case 3: return 5;
    case 4: return 10;
    default: return -1;
  }
}

// Minimum public header size per minor version.
std::uint16_t minHeaderSize(int minor) {
  if (minor <= 2) return 227;
  if (minor == 3) return 235;
  return 375;
}

}  // namespace

RawCloud readLas(const std::string& path) {
  if (!hostLittleEndian()) throw std::runtime_error("LAS reader requires a little-endian host");
  std::ifstream in(path, std::ios::binary);
  if (!in) throw std::runtime_error("cannot open " + path);
  in.seekg(0, std::ios::end);
  const std::uint64_t file_size = static_cast<std::uint64_t>(in.tellg());
  in.seekg(0);
  if (file_size < 227) throw std::runtime_error(path + ": file too small for a LAS header");

  std::uint8_t hdr[375] = {0};
  const std::size_t to_read = static_cast<std::size_t>(std::min<std::uint64_t>(file_size, 375));
  in.read(reinterpret_cast<char*>(hdr), static_cast<std::streamsize>(to_read));
  if (std::memcmp(hdr, "LASF", 4) != 0) throw std::runtime_error(path + ": not a LAS file (missing LASF signature)");

  const int major = hdr[24], minor = hdr[25];
  if (major != 1 || minor > 4)
    throw std::runtime_error(path + ": unsupported LAS version " + std::to_string(major) + "." + std::to_string(minor) +
                             " (supported 1.0-1.4)");
  const auto header_size = get<std::uint16_t>(hdr, 94);
  const auto offset_to_points = get<std::uint32_t>(hdr, 96);
  const std::uint8_t format_byte = hdr[104];
  const auto record_len = get<std::uint16_t>(hdr, 105);
  const auto legacy_count = get<std::uint32_t>(hdr, 107);

  if (header_size < minHeaderSize(minor))
    throw std::runtime_error(path + ": header size " + std::to_string(header_size) + " too small for LAS 1." +
                             std::to_string(minor));
  if (format_byte & 0xC0)
    throw std::runtime_error(path + ": point data is compressed (LAZ, format byte " + std::to_string(format_byte) +
                             "). Decompress to plain LAS first.");
  const int format = format_byte;
  if (format > 10)
    throw std::runtime_error(path + ": unsupported point data record format " + std::to_string(format) +
                             " (supported 0-10)");
  if (format > maxFormatForMinor(minor))
    throw std::runtime_error(path + ": point format " + std::to_string(format) + " is not valid in LAS 1." +
                             std::to_string(minor) + " (max " + std::to_string(maxFormatForMinor(minor)) + ")");
  if (record_len < kMinRecordLength[format])
    throw std::runtime_error(path + ": record length " + std::to_string(record_len) + " < minimum " +
                             std::to_string(kMinRecordLength[format]) + " for format " + std::to_string(format));
  if (offset_to_points < header_size) throw std::runtime_error(path + ": offset to point data inside header");

  std::uint64_t count = legacy_count;
  if (minor >= 4) {
    const auto count64 = get<std::uint64_t>(hdr, 247);
    if (count64 != 0) count = count64;
    if (legacy_count != 0 && count64 != 0 && legacy_count != count64)
      throw std::runtime_error(path + ": legacy point count and 64-bit point count disagree");
  }
  if (count == 0) throw std::runtime_error(path + ": LAS header reports zero points");
  const std::uint64_t needed = static_cast<std::uint64_t>(offset_to_points) + count * record_len;
  if (needed > file_size)
    throw std::runtime_error(path + ": truncated: header promises " + std::to_string(count) + " points (" +
                             std::to_string(needed) + " bytes), file has " + std::to_string(file_size));

  const double sx = get<double>(hdr, 131), sy = get<double>(hdr, 139), sz = get<double>(hdr, 147);
  const double ox = get<double>(hdr, 155), oy = get<double>(hdr, 163), oz = get<double>(hdr, 171);
  if (!(sx > 0) || !(sy > 0) || !(sz > 0) || !std::isfinite(ox) || !std::isfinite(oy) || !std::isfinite(oz))
    throw std::runtime_error(path + ": invalid scale/offset in LAS header");

  RawCloud rc;
  rc.format = "las";
  rc.detail = "LAS 1." + std::to_string(minor) + " format " + std::to_string(format) + ", record length " +
              std::to_string(record_len);
  rc.points.reserve(static_cast<std::size_t>(count));
  in.seekg(offset_to_points);
  constexpr std::uint64_t kChunk = 65536;
  std::vector<std::uint8_t> buf;
  for (std::uint64_t done = 0; done < count;) {
    const std::uint64_t n = std::min(kChunk, count - done);
    buf.resize(static_cast<std::size_t>(n * record_len));
    in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!in) throw std::runtime_error(path + ": read error in point data");
    for (std::uint64_t i = 0; i < n; ++i) {
      const std::uint8_t* r = buf.data() + i * record_len;
      rc.points.emplace_back(get<std::int32_t>(r, 0) * sx + ox, get<std::int32_t>(r, 4) * sy + oy,
                             get<std::int32_t>(r, 8) * sz + oz);
    }
    done += n;
  }
  return rc;
}

}  // namespace cm
