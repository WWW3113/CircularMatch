// Writes the default synthetic scene (sloped ground + 5 stems, 2 tilted).
//   cm_synth --out=scene.las|scene.xyz --truth=truth.csv [--offset_x=... --offset_y=... --offset_z=...]
#include <fstream>
#include <iostream>

#include "app_common.hpp"
#include "cm/synth.hpp"

using namespace cm;

int main(int argc, char** argv) {
  try {
    auto a = app::parseArgs(argc, argv, {"out", "truth", "offset_x", "offset_y", "offset_z", "seed", "help"});
    if (a.opts.count("help") || !a.opts.count("out")) {
      std::cout << "usage: cm_synth --out=FILE.las|FILE.xyz [--truth=truth.csv] [--offset_x=..] [--seed=..]\n";
      return a.opts.count("help") ? 0 : 2;
    }
    SynthParams p = SynthParams::defaultScene();
    if (a.opts.count("offset_x")) p.offset.x() = std::stod(a.opts["offset_x"]);
    if (a.opts.count("offset_y")) p.offset.y() = std::stod(a.opts["offset_y"]);
    if (a.opts.count("offset_z")) p.offset.z() = std::stod(a.opts["offset_z"]);
    if (a.opts.count("seed")) p.seed = std::stoull(a.opts["seed"]);
    std::vector<SynthTruth> truth;
    const RawCloud rc = makeSynthetic(p, &truth);
    const std::string out = a.opts["out"];
    if (out.size() > 4 && out.substr(out.size() - 4) == ".las")
      writeLas(out, rc.points, 2, 0, 0.0005, Eigen::Vector3d(std::floor(p.offset.x()), std::floor(p.offset.y()),
                                                                std::floor(p.offset.z())));
    else
      writeXyz(out, rc.points);
    std::cout << "wrote " << rc.points.size() << " points to " << out << " (scanner at " << p.offset.transpose()
              << ")\n";
    if (a.opts.count("truth")) {
      std::ofstream t(a.opts["truth"]);
      t.setf(std::ios::fixed);
      t.precision(4);
      t << "x,y,z,radius,tilt_deg\n";
      for (const auto& s : truth)
        t << s.position.x() << "," << s.position.y() << "," << s.position.z() << "," << s.radius << "," << s.tilt_deg
          << "\n";
    }
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "ERROR: " << e.what() << "\n";
    return 1;
  }
}
