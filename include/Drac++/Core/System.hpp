#pragma once

#if DRAC_ENABLE_WEATHER
  #include "Drac++/Services/Weather.hpp"
#endif

#include "DracUtils/Definitions.hpp"
#include "DracUtils/Error.hpp"
#include "DracUtils/Types.hpp"

/// @brief Conversion factor from bytes to GiB
constexpr util::types::u64 GIB = 1'073'741'824;

/**
 * @struct BytesToGiB
 * @brief Helper struct to format a byte value to GiB (Gibibytes).
 *
 * Encapsulates a byte value and provides a custom formatter
 * to convert it to GiB for display purposes.
 */
struct BytesToGiB {
  util::types::u64 value; ///< The byte value to be converted.

  /**
   * @brief Constructor for BytesToGiB.
   * @param value The byte value to be converted.
   */
  explicit constexpr BytesToGiB(const util::types::u64 value) : value(value) {}
};

/**
 * @brief Custom formatter for BytesToGiB.
 *
 * Allows formatting BytesToGiB values using std::format.
 * Outputs the value in GiB with two decimal places.
 *
 * @code{.cpp}
 * #include <format>
 * #include "system_data.h"
 *
 * int main() {
 *   BytesToGiB data_size{2'147'483'648}; // 2 GiB
 *   std::string formatted = std::format("Size: {}", data_size);
 *   std::println("{}", formatted); // formatted will be "Size: 2.00GiB"
 *   return 0;
 * }
 * @endcode
 */
template <>
struct std::formatter<BytesToGiB> : std::formatter<double> {
  /**
   * @brief Formats the BytesToGiB value.
   * @param BTG The BytesToGiB instance to format.
   * @param ctx The formatting context.
   * @return An iterator to the end of the formatted output.
   */
  fn format(const BytesToGiB& BTG, auto& ctx) const {
    return std::format_to(ctx.out(), "{:.2f}GiB", static_cast<util::types::f64>(BTG.value) / GIB);
  }
};

namespace os {
  /**
   * @class System
   * @brief Holds various pieces of system information collected from the OS,
   *        and provides methods to fetch this information.
   *
   * This class aggregates information about the system,
   * in order to display it at all at once during runtime.
   * The actual implementation for each fetch function is platform-specific.
   */
  class System {
   public:
    util::types::Result<util::types::String>        date;          ///< Current date (e.g., "April 26th").
    util::types::Result<util::types::String>        host;          ///< Host/product family (e.g., "MacBook Air").
    util::types::Result<util::types::String>        kernelVersion; ///< OS kernel version (e.g., "6.14.4").
    util::types::Result<util::types::String>        osVersion;     ///< OS pretty name (e.g., "Ubuntu 24.04.2 LTS").
    util::types::Result<util::types::ResourceUsage> memInfo;       ///< Total physical RAM in bytes.
    util::types::Result<util::types::String>        desktopEnv;    ///< Desktop environment (e.g., "KDE").
    util::types::Result<util::types::String>        windowMgr;     ///< Window manager (e.g., "KWin").
    util::types::Result<util::types::ResourceUsage> diskUsage;     ///< Used/Total disk space for root filesystem.
    util::types::Result<util::types::String>        shell;         ///< Name of the current user shell (e.g., "zsh").
    util::types::Result<util::types::String>        cpuModel;      ///< CPU model name.
    util::types::Result<util::types::String>        gpuModel;      ///< GPU model name.
#if DRAC_ENABLE_PACKAGECOUNT
    util::types::Result<util::types::u64> packageCount; ///< Total number of packages installed.
#endif
#if DRAC_ENABLE_NOWPLAYING
    util::types::Result<util::types::MediaInfo> nowPlaying; ///< Result of fetching media info.
#endif
#if DRAC_ENABLE_WEATHER
    util::types::Result<weather::Report> weather; ///< Result of fetching weather info.
#endif

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
    static fn getMemInfo() -> util::types::Result<util::types::ResourceUsage>;

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
    static fn getNowPlaying() -> util::types::Result<util::types::MediaInfo>;
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
    static fn getOSVersion() -> util::types::Result<util::types::String>;

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
    static fn getDesktopEnvironment() -> util::types::Result<util::types::String>;

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
    static fn getWindowManager() -> util::types::Result<util::types::String>;

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
    static fn getShell() -> util::types::Result<util::types::String>;

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
    static fn getHost() -> util::types::Result<util::types::String>;

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
    static fn getCPUModel() -> util::types::Result<util::types::String>;

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
    static fn getGPUModel() -> util::types::Result<util::types::String>;

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
    static fn getKernelVersion() -> util::types::Result<util::types::String>;

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
    static fn getDiskUsage() -> util::types::Result<util::types::ResourceUsage>;
  };
} // namespace os
