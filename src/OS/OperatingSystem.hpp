#pragma once

#include "Util/Definitions.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

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
  using util::types::u64, util::types::String, util::types::Result, util::types::MediaInfo, util::types::DiskSpace;

  /**
   * @brief Get the total amount of physical RAM installed in the system.
   * @return A Result containing the total RAM in bytes (u64) on success.
   */
  fn GetMemInfo() -> Result<u64>;

  /**
   * @brief Gets structured metadata about the currently playing media.
   * @return A Result containing the media information (MediaInfo struct) on success.
   */
  fn GetNowPlaying() -> Result<MediaInfo>;

  /**
   * @brief Gets the "pretty" name of the operating system.
   * @details Examples: "Ubuntu 24.04.2 LTS", "Windows 11 Pro 24H2", "macOS 15 Sequoia".
   * @return A Result containing the OS version String on success.
   */
  fn GetOSVersion() -> Result<String>;

  /**
   * @brief Attempts to retrieve the desktop environment name.
   * @details This is most relevant on Linux. May check environment variables (XDG_CURRENT_DESKTOP), session files,
   * or running processes. On Windows/macOS, it might return a UI theme identifier (e.g., "Fluent", "Aqua") or None.
   * @return A Result containing the DE name String on success.
   */
  fn GetDesktopEnvironment() -> Result<String>;

  /**
   * @brief Attempts to retrieve the window manager name.
   * @details On Linux, checks Wayland compositor or X11 WM properties. On Windows, returns "DWM" or similar.
   * On macOS, might return "Quartz Compositor" or a specific tiling WM name if active.
   * @return A Result containing the detected WM name String on success.
   */
  fn GetWindowManager() -> Result<String>;

  /**
   * @brief Attempts to detect the current user shell name.
   * @details Checks the SHELL environment variable on Linux/macOS. On Windows, inspects the process tree
   * to identify known shells like PowerShell, Cmd, or MSYS2 shells (Bash, Zsh).
   * @return A Result containing the shell name String on success.
   */
  fn GetShell() -> Result<String>;

  /**
   * @brief Gets a system identifier, often the hardware model or product family.
   * @details Examples: "MacBookPro18,3", "Latitude 5420", "ThinkPad T490".
   * Implementation varies: reads DMI info on Linux, registry on Windows, sysctl on macOS.
   * @return A Result containing the host/product identifier String on success.
   */
  fn GetHost() -> Result<String>;

  /**
   * @brief Gets the operating system's kernel version string.
   * @details Examples: "5.15.0-76-generic", "10.0.22621", "23.1.0".
   * Uses uname() on Linux/macOS, WinRT/registry on Windows.
   * @return A Result containing the kernel version String on success.
   */
  fn GetKernelVersion() -> Result<String>;

  /**
   * @brief Gets the disk usage for the primary/root filesystem.
   * @details Uses statvfs on Linux/macOS, GetDiskFreeSpaceExW on Windows.
   * @return A Result containing the DiskSpace struct (used/total bytes) on success.
   */
  fn GetDiskUsage() -> Result<DiskSpace>;
} // namespace os
