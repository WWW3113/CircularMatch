#include "app_common.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace cm::app {

Args parseArgs(int argc, char** argv, const std::vector<std::string>& allowed) {
  Args a;
  std::vector<std::string> args(argv + 1, argv + argc);
  for (const auto& s : args)
    if (s.rfind("--config=", 0) == 0) a.cfg.loadIni(s.substr(9));
  for (const auto& s : a.cfg.applyArgs(args)) {
    if (s.rfind("--config=", 0) == 0) continue;
    if (s.rfind("--", 0) != 0) throw std::runtime_error("unexpected argument '" + s + "'");
    const auto eq = s.find('=');
    const std::string k = s.substr(2, eq == std::string::npos ? std::string::npos : eq - 2);
    if (std::find(allowed.begin(), allowed.end(), k) == allowed.end())
      throw std::runtime_error("unknown option '--" + k + "'");
    a.opts[k] = eq == std::string::npos ? "1" : s.substr(eq + 1);
  }
  return a;
}

void ensureDir(const std::string& dir) {
  if (!dir.empty()) std::filesystem::create_directories(dir);
}

void writeConfigDump(const Config& cfg, const std::string& path) {
  std::ofstream o(path);
  if (!o) throw std::runtime_error("cannot write " + path);
  cfg.dump(o);
}

}  // namespace cm::app
