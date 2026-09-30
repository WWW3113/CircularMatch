#pragma once
#include <map>
#include <streambuf>
#include <string>
#include <vector>

#include "cm/config.hpp"

namespace cm::app {

// Parses argv: "--config=file.ini" is loaded first, then "--section.key=value"
// overrides; remaining "--name=value" options are returned in `opts`.
// Throws on unknown options not listed in `allowed`.
struct Args {
  Config cfg;
  std::map<std::string, std::string> opts;
};
Args parseArgs(int argc, char** argv, const std::vector<std::string>& allowed);
// Writes to one or two stream buffers (second may be null).
class TeeBuf : public std::streambuf {
 public:
  TeeBuf(std::streambuf* a, std::streambuf* b) : a_(a), b_(b) {}

 protected:
  int overflow(int c) override {
    if (c == EOF) return !EOF;
    if (a_->sputc(char(c)) == EOF) return EOF;
    if (b_ && b_->sputc(char(c)) == EOF) return EOF;
    return c;
  }
  int sync() override { return (a_->pubsync() == 0 && (!b_ || b_->pubsync() == 0)) ? 0 : -1; }

 private:
  std::streambuf *a_, *b_;
};

void ensureDir(const std::string& dir);
void writeConfigDump(const Config& cfg, const std::string& path);

}  // namespace cm::app
