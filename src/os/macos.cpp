#ifdef __APPLE__

#include <sys/sysctl.h>

#include "macos/now_playing_bridge.h"
#include "os.h"

u64 GetMemInfo() {
  u64 mem    = 0;
  usize size = sizeof(mem);

  sysctlbyname("hw.memsize", &mem, &size, nullptr, 0);

  return mem;
}

std::string GetNowPlaying() {
  if (const char* title  = GetCurrentPlayingTitle();
      const char* artist = GetCurrentPlayingArtist())
    return "Now Playing: " + std::string(artist) + " - " + std::string(title);

  return "No song playing";
}

#endif
