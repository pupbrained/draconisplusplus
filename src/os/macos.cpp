#include "os.h"
#include <sys/sysctl.h>

uint64_t get_meminfo() {
  uint64_t mem = 0;
  size_t size = sizeof(mem);

  sysctlbyname("hw.memsize", &mem, &size, nullptr, 0);

  return mem;
}
