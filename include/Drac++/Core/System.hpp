/**
 * @file System.hpp
 * @brief Defines the os::System class, a cross-platform interface for querying system information.
 * @author pupbrained/Draconis
 * @version DRACONISPLUSPLUS_VERSION
 */

#pragma once

#include "../Utils/Definitions.hpp"
#include "../Utils/Error.hpp"
#include "../Utils/Types.hpp"

namespace draconis::core::system {
  namespace {
    using utils::types::f64;
    using utils::types::i64;
    using utils::types::MediaInfo;
    using utils::types::ResourceUsage;
    using utils::types::Result;
    using utils::types::String;
    using utils::types::u64;
  } // namespace

  /**
   * @brief Fetches memory information.
   * @return The ResourceUsage struct containing the used and total memory in bytes.
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: `GlobalMemoryStatusEx`
   *  - macOS: `host_statistics64` / `sysctlbyname("hw.memsize")`
   *  - Linux: `sysinfo`
   *  - FreeBSD/DragonFly: `sysctlbyname("hw.physmem")`
   *  - NetBSD: `sysctlbyname("hw.physmem64")`
   *  - Haiku: `get_system_info`
   *  - SerenityOS: Reads from `/sys/kernel/memstat`
   *
   * @warning This function can fail if:
   *  - Windows: `GlobalMemoryStatusEx` fails
   *  - macOS: `host_page_size` fails / `sysctlbyname` returns -1 / `host_statistics64` fails
   *  - Linux: `sysinfo` fails
   *  - FreeBSD/DragonFly: `sysctlbyname` returns -1
   *  - NetBSD: `sysctlbyname` returns -1
   *  - Haiku: `get_system_info` fails
   *  - SerenityOS: `/sys/kernel/memstat` fails to open / `glz::read` fails to parse JSON / `physical_allocated` + `physical_available` overflows
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<ResourceUsage> memInfo = os::System::getMemInfo();
   *
   *   if (memInfo.has_value()) {
   *     std::println("Used: {} bytes", memInfo.value().usedBytes);
   *     std::println("Total: {} bytes", memInfo.value().totalBytes);
   *   } else {
   *     std::println("Failed to get memory info: {}", memInfo.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetMemInfo() -> Result<ResourceUsage>;

#if DRAC_ENABLE_NOWPLAYING
  /**
   * @brief Fetches now playing media information.
   * @return The MediaInfo struct containing the title and artist of the currently playing media.
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: `GlobalSystemMediaTransportControlsSessionManager::GetCurrentSession`
   *  - macOS: `MRMediaRemoteGetNowPlayingInfo` (private framework)
   *  - Linux/BSD: `DBus`
   *  - Other: Unsupported
   *
   * @warning This function can fail if:
   *  - Windows: `GlobalSystemMediaTransportControlsSessionManager::GetCurrentSession` fails
   *  - macOS: `MRMediaRemoteGetNowPlayingInfo` fails / `information` is null
   *  - Linux/BSD: Various DBus calls fail
   *  - Other: Unsupported
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<MediaInfo> nowPlaying = os::System::getNowPlaying();
   *
   *   if (nowPlaying.has_value()) {
   *     std::println("Now playing: {} - {}", nowPlaying.value().title, nowPlaying.value().artist);
   *   } else {
   *     std::println("Failed to get now playing: {}", nowPlaying.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetNowPlaying() -> Result<MediaInfo>;
#endif

  /**
   * @brief Fetches the OS version.
   * @return The OS version (e.g., "Windows 11", "macOS 26.0 Tahoe", "Ubuntu 24.04.2 LTS", etc.).
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: Reads from `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion`, returns "Windows 11" if `buildNumber >= 22000`
   *  - macOS: Parses `/System/Library/CoreServices/SystemVersion.plist`, matches against a map of known versions
   *  - Linux: Parses `PRETTY_NAME` from `/etc/os-release`
   *  - BSD: Parses `NAME` from `/etc/os-release`, falls back to `uname`
   *  - Haiku: Reads from `/boot/system/lib/libbe.so`
   *  - SerenityOS: `uname`
   *
   * @warning This function can fail if:
   *  - Windows: `RegOpenKeyExW` fails / `productName` is empty
   *  - macOS: Various CF functions fail / `versionNumber` is empty / `versionNumber` is not a valid version number
   *  - Linux: Fails to open `/etc/os-release` / fails to find/parse `PRETTY_NAME`
   *  - BSD: Fails to open `/etc/os-release` / fails to find/parse `NAME` and `uname` returns -1 / `uname` returns empty string
   *  - Haiku: Fails to open `/boot/system/lib/libbe.so` / `BAppFileInfo::SetTo` fails / `appInfo.GetVersionInfo` fails / `versionShortString` is empty
   *  - SerenityOS: `uname` returns -1
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> osVersion = os::System::getOSVersion();
   *
   *   if (osVersion.has_value()) {
   *     std::println("OS version: {}", osVersion.value());
   *   } else {
   *     std::println("Failed to get OS version: {}", osVersion.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetOSVersion() -> Result<String>;

  /**
   * @brief Fetches the desktop environment.
   * @return The desktop environment (e.g., "KDE", "Aqua", "Fluent (Windows 11)", etc.).
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: UI design language based on build number in `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\CurrentBuildNumber`
   *  - macOS: Hardcoded to "Aqua"
   *  - Haiku: Hardcoded to "Haiku Desktop Environment"
   *  - SerenityOS: Hardcoded to "SerenityOS Desktop"
   *  - Other: `XDG_CURRENT_DESKTOP` environment variable, falls back to `DESKTOP_SESSION` environment variable
   *
   * @warning This function can fail if:
   *  - Windows: `RegOpenKeyExW` fails / `buildStr` is empty / `stoi` fails
   *  - macOS/Haiku/SerenityOS: N/A (hardcoded)
   *  - Other: `GetEnv` fails
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> desktopEnv = os::System::getDesktopEnvironment();
   *
   *   if (desktopEnv.has_value()) {
   *     std::println("Desktop environment: {}", desktopEnv.value());
   *   } else {
   *     std::println("Failed to get desktop environment: {}", desktopEnv.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetDesktopEnvironment() -> Result<String>;

  /**
   * @brief Fetches the window manager.
   * @return The window manager (e.g, "KWin", "yabai", "DWM", etc.).
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: "DWM" (if `DwmIsCompositionEnabled` succeeds) / "Windows Manager (Basic)" (if `DwmIsCompositionEnabled` fails)
   *  - macOS: Checks for known window managers in the process tree, falls back to "Quartz"
   *  - Haiku: Hardcoded to "app_server"
   *  - SerenityOS: Hardcoded to "WindowManager"
   *  - Other: Gets X11 or Wayland window manager depending on whether `WAYLAND_DISPLAY` or `DISPLAY` is set
   *
   * @warning This function can fail if:
   *  - Windows: `DwmIsCompositionEnabled` fails
   *  - macOS: `sysctl` returns -1, zero length, or a size that is not a multiple of `kinfo_proc` size / `Capitalize` fails
   *  - Haiku/SerenityOS: N/A (hardcoded)
   *  - Other: If DRAC_ENABLE_X11/DRAC_ENABLE_WAYLAND are disabled / `GetX11WindowManager`/`GetWaylandCompositor` fails
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> windowMgr = os::System::getWindowManager();
   *
   *   if (windowMgr.has_value()) {
   *     std::println("Window manager: {}", windowMgr.value());
   *   } else {
   *     std::println("Failed to get window manager: {}", windowMgr.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetWindowManager() -> Result<String>;

  /**
   * @brief Fetches the shell.
   * @return The active shell (e.g., "zsh", "bash", "fish", etc.).
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: `MSYSTEM` variable / `SHELL` variable / `LOGINSHELL` variable / process tree
   *  - SerenityOS: `getpwuid`
   *  - Other: `SHELL` variable
   *
   * @warning This function can fail if:
   *  - Windows: None of `MSYSTEM` / `SHELL` / `LOGINSHELL` variables are set and process tree check fails
   *  - SerenityOS: `pw` is null / `pw_shell` is null or empty
   *  - Other: `SHELL` variable is not set
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> shell = os::System::getShell();
   *
   *   if (shell.has_value()) {
   *     std::println("Shell: {}", shell.value());
   *   } else {
   *     std::println("Failed to get shell: {}", shell.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetShell() -> Result<String>;

  /**
   * @brief Fetches the host.
   * @return The host (or hostname if the platform doesn't support the former).
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: Reads from `HKEY_LOCAL_MACHINE\SYSTEM\HardwareConfig\Current`
   *  - macOS: `sysctlbyname("hw.model")` - matched against a flat_map of known models
   *  - Linux: Reads from `/sys/class/dmi/id/product_family`, falls back to `/sys/class/dmi/id/product_name`
   *  - FreeBSD/DragonFly: `kenv smbios.system.product`, falls back to `sysctlbyname("hw.model")`
   *  - NetBSD: `sysctlbyname("machdep.dmi.system-product")`
   *  - Haiku: `gethostname`
   *  - SerenityOS: `gethostname`
   *
   * @warning This function can fail if:
   *  - Windows: `RegOpenKeyExW` fails
   *  - macOS: `sysctlbyname` returns -1 / model not found in known models
   *  - Linux: `/sys/class/dmi/id/product_family` and `/sys/class/dmi/id/product_name` fail to read
   *  - FreeBSD/DragonFly: `kenv` returns -1 and `sysctlbyname` returns -1 / empty string
   *  - NetBSD: `sysctlbyname` returns -1
   *  - Haiku: `gethostname` returns non-zero
   *  - SerenityOS: `gethostname` returns non-zero
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> host = os::System::getHost();
   *
   *   if (host.has_value()) {
   *     std::println("Host: {}", host.value());
   *   } else {
   *     std::println("Failed to get host: {}", host.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetHost() -> Result<String>;

  /**
   * @brief Fetches the CPU model.
   * @return The CPU model (e.g., "Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz").
   *
   * @details Obtained differently depending on the platform and architecture:
   *  - Windows: `__cpuid` (x86) / `RegQueryValueExW` (arm64)
   *  - macOS: `sysctlbyname("machdep.cpu.brand_string")`
   *  - Linux: `__get_cpuid` (x86) / `/proc/cpuinfo` (arm64)
   *  - Other: To be implemented
   *
   * @warning This function can fail if:
   *  - Windows: `__cpuid` fails (x86) / `RegOpenKeyExW` fails (arm64)
   *  - macOS: `sysctlbyname` fails
   *  - Linux: `__get_cpuid` fails (x86) / `/proc/cpuinfo` is empty (arm64)
   *  - Other: To be implemented
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> cpuModel = os::System::getCPUModel();
   *
   *   if (cpuModel.has_value()) {
   *     std::println("CPU model: {}", cpuModel.value());
   *   } else {
   *     std::println("Failed to get CPU model: {}", cpuModel.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetCpuModel() -> Result<String>;

  /**
   * @brief Fetches the GPU model.
   * @return The GPU model (e.g., "NVIDIA GeForce RTX 3070").
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: DXGI
   *  - macOS: Metal
   *  - Other: To be implemented
   *
   * @warning This function can fail if:
   *  - Windows: `CreateDXGIFactory` / `pFactory->EnumAdapters` / `pAdapter->GetDesc` fails
   *  - macOS: `MTLCreateSystemDefaultDevice` fails / `device.name` is null
   *  - Other: To be implemented
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> gpuModel = os::System::getGPUModel();
   *
   *   if (gpuModel.has_value()) {
   *     std::println("GPU model: {}", gpuModel.value());
   *   } else {
   *     std::println("Failed to get GPU model: {}", gpuModel.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetGpuModel() -> Result<String>;

  /**
   * @brief Fetches the kernel version.
   * @return The kernel version (e.g., "6.14.4").
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: "10.0.22000" (From `KUSER_SHARED_DATA`)
   *  - macOS: "22.3.0" (`sysctlbyname("kern.osrelease")`)
   *  - Haiku: "1" (`get_system_info`)
   *  - Other Unix-like systems: "6.14.4" (`uname`)
   *
   * @warning This function can fail if:
   *  - Windows: `kuserSharedData` fails to parse / `__try` fails
   *  - macOS: `sysctlbyname` returns -1
   *  - Haiku: `get_system_info` returns anything other than `B_OK`
   *  - Other Unix-like systems: `uname` returns -1 / `uname` returns empty string
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<String> kernelVersion = os::System::getKernelVersion();
   *
   *   if (kernelVersion.has_value()) {
   *     std::println("Kernel version: {}", kernelVersion.value());
   *   } else {
   *     std::println("Failed to get kernel version: {}", kernelVersion.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetKernelVersion() -> Result<String>;

  /**
   * @brief Fetches the disk usage.
   * @return The ResourceUsage struct containing the used and total disk space in bytes.
   *
   * @details Obtained differently depending on the platform:
   *  - Windows: `GetDiskFreeSpaceExW`
   *  - Other: `statvfs`
   *
   * @warning This function can fail if:
   *  - Windows: `GetDiskFreeSpaceExW` fails
   *  - Other: `statvfs` returns -1
   *
   * @code{.cpp}
   * #include <print>
   * #include <Drac++/Core/System.hpp>
   *
   * int main() {
   *   Result<ResourceUsage> diskUsage = os::System::getDiskUsage();
   *
   *   if (diskUsage.has_value()) {
   *     std::println("Used: {} bytes", diskUsage.value().usedBytes);
   *     std::println("Total: {} bytes", diskUsage.value().totalBytes);
   *   } else {
   *     std::println("Failed to get disk usage: {}", diskUsage.error().message());
   *   }
   *
   *   return 0;
   * }
   * @endcode
   */
  fn GetDiskUsage() -> Result<ResourceUsage>;
} // namespace draconis::core::system
