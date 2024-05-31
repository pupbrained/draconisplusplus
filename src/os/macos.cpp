#include <sys/sysctl.h>
#include <unistd.h>
#include <string>

uint64_t get_meminfo() {
  uint64_t mem = 0;
  size_t size = sizeof(mem);

  sysctlbyname("hw.memsize", &mem, &size, nullptr, 0);

  return mem;
}

static std::string get_nowplaying() {
  return "";
}
