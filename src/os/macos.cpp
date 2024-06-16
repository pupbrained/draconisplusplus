#ifdef __APPLE__

#include <sys/sysctl.h>

#include "macos/bridge.h"
#include "os.h"

fn GetMemInfo() -> u64 {
  u64   mem  = 0;
  usize size = sizeof(mem);

  sysctlbyname("hw.memsize", &mem, &size, nullptr, 0);

  return mem;
}

fn GetNowPlaying() -> std::string {
  if (const char* title = GetCurrentPlayingTitle(); const char* artist = GetCurrentPlayingArtist())
    return "Now Playing: " + std::string(artist) + " - " + std::string(title);

  return "No song playing";
}

fn GetOSVersion() -> const char* { return GetMacOSVersion(); };

#endif
