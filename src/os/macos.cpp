#ifdef __APPLE__

#include <sys/sysctl.h>
#include <string>
#include "os.h"

uint64_t GetMemInfo() {
  uint64_t mem = 0;
  size_t size = sizeof(mem);

  sysctlbyname("hw.memsize", &mem, &size, nullptr, 0);

  return mem;
}

std::string GetNowPlaying() {
  return "";
}

#endif
