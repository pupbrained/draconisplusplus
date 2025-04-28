#pragma once

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"

/**
 * @namespace os
 * @brief Provides a platform-abstracted interface for retrieving Operating System information.
 *
 * This namespace declares functions to get various system details like memory,
 * OS version, hardware identifiers, media playback status, etc.
 * The actual implementation for each function is platform-specific
 * (found in linux.cpp, windows.cpp, macos.cpp).
 */
namespace os {
  using util::error::DraconisError;
  using util::types::u64, util::types::String, util::types::Option, util::types::Result, util::types::MediaInfo,
    util::types::DiskSpace;

  /**
   * @brief Get the total amount of physical RAM installed in the system.
   * @return A Result containing the total RAM in bytes (u64) on success,
   * or an OsError on failure.
   */
  fn GetMemInfo() -> Result<u64, DraconisError>;

  /**
   * @brief Gets structured metadata about the currently playing media.
   * @return A Result containing the media information (MediaInfo struct) on success,
   * or a NowPlayingError (indicating player state or system error) on failure.
   */
  fn GetNowPlaying() -> Result<MediaInfo, DraconisError>;

  /**
   * @brief Gets the "pretty" name of the operating system.
   * @details Examples: "Ubuntu 24.04.2 LTS", "Windows 11 Pro 24H2", "macOS 15 Sequoia".
   * @return A Result containing the OS version String on success, or an OsError on failure.
   */
  fn GetOSVersion() -> Result<String, DraconisError>;

  /**
   * @brief Attempts to retrieve the desktop environment name.
   * @details This is most relevant on Linux. May check environment variables (XDG_CURRENT_DESKTOP), session files,
   * or running processes. On Windows/macOS, it might return a UI theme identifier (e.g., "Fluent", "Aqua") or None.
   * @return An Option containing the detected DE name String, or None if detection fails or is not applicable.
   */
  fn GetDesktopEnvironment() -> Option<String>;

  /**
   * @brief Attempts to retrieve the window manager name.
   * @details On Linux, checks Wayland compositor or X11 WM properties. On Windows, returns "DWM" or similar.
   * On macOS, might return "Quartz Compositor" or a specific tiling WM name if active.
   * @return An Option containing the detected WM name String, or None if detection fails.
   */
  fn GetWindowManager() -> Option<String>;

  /**
   * @brief Attempts to detect the current user shell name.
   * @details Checks the SHELL environment variable on Linux/macOS. On Windows, inspects the process tree
   * to identify known shells like PowerShell, Cmd, or MSYS2 shells (Bash, Zsh).
   * @return An Option containing the detected shell name (e.g., "Bash", "Zsh", "PowerShell", "Fish"),
   * or None if detection fails.
   */
  fn GetShell() -> Option<String>;

  /**
   * @brief Gets a system identifier, often the hardware model or product family.
   * @details Examples: "MacBookPro18,3", "Latitude 5420", "ThinkPad T490".
   * Implementation varies: reads DMI info on Linux, registry on Windows, sysctl on macOS.
   * @return A Result containing the host/product identifier String on success,
   * or an OsError on failure (e.g., permission reading DMI/registry, API error).
   */
  fn GetHost() -> Result<String, DraconisError>;

  /**
   * @brief Gets the operating system's kernel version string.
   * @details Examples: "5.15.0-76-generic", "10.0.22621", "23.1.0".
   * Uses uname() on Linux/macOS, WinRT/registry on Windows.
   * @return A Result containing the kernel version String on success,
   * or an OsError on failure.
   */
  fn GetKernelVersion() -> Result<String, DraconisError>;

  /**
   * @brief Gets the number of installed packages (Platform-specific).
   * @details On Linux, sums counts from various package managers. On other platforms, behavior may vary.
   * @return A Result containing the package count (u64) on success,
   * or an OsError on failure (e.g., permission errors, command not found)
   * or if not supported (OsErrorCode::NotSupported).
   */
  fn GetPackageCount() -> Result<u64, DraconisError>;

  /**
   * @brief Gets the disk usage for the primary/root filesystem.
   * @details Uses statvfs on Linux/macOS, GetDiskFreeSpaceExW on Windows.
   * @return A Result containing the DiskSpace struct (used/total bytes) on success,
   * or an OsError on failure (e.g., filesystem not found, permission error).
   */
  fn GetDiskUsage() -> Result<DiskSpace, DraconisError>;
} // namespace os
