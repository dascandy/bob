#ifndef PROFILE_H
#define PROFILE_H

#include <cstdio>
#include <chrono>

#define PROFILE(s) Profile scope(#s, __LINE__, verbose);

class Profile {
public:
  Profile(const char* const desc, int line, bool verbose)
  : timestamp(std::chrono::high_resolution_clock::now())
  , text(desc)
  , line(line)
  , verbose(verbose)
  {}
  ~Profile() {
    if (verbose) {
      std::chrono::nanoseconds duration = std::chrono::high_resolution_clock::now() - timestamp;
      std::printf("PROFILE %s:%lu: => %lu\n", text, line, duration.count());
    }
  }
private:
  std::chrono::high_resolution_clock::time_point timestamp;
  const char *text;
  unsigned long line;
  bool verbose;
};

#endif


