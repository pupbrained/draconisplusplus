#ifdef __APPLE__

#include <map>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#include "macos/bridge.h"
#include "os.h"

fn GetMemInfo() -> u64 {
  u64   mem  = 0;
  usize size = sizeof(mem);

  sysctlbyname("hw.memsize", &mem, &size, nullptr, 0);

  return mem;
}

fn GetNowPlaying() -> string {
  if (const char* title = GetCurrentPlayingTitle(); const char* artist = GetCurrentPlayingArtist())
    return "Now Playing: " + string(artist) + " - " + string(title);

  return "No song playing";
}

fn GetOSVersion() -> string { return GetMacOSVersion(); }

fn GetDesktopEnvironment() -> string { return "Aqua"; }

fn GetKernelVersion() -> string {
  struct utsname uts;

  if (uname(&uts) == -1) {
    ERROR_LOG("uname() failed: {}", std::strerror(errno));
    return "";
  }

  return static_cast<const char*>(uts.release);
}

fn GetHost() -> string {
  std::array<char, 256> hwModel;
  size_t                hwModelLen = sizeof(hwModel);

  sysctlbyname("hw.model", hwModel.data(), &hwModelLen, nullptr, 0);

  // shamelessly stolen from https://github.com/fastfetch-cli/fastfetch/blob/dev/src/detection/host/host_mac.c
  std::map<string, string> modelNameByHwModel = {
    { "Mac14,2", "MacBook Air (M2, 2022)" }
  };

  return modelNameByHwModel[hwModel.data()];
}

#endif
