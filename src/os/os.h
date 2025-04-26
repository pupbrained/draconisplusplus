#pragma once

#include "../util/macros.h"
#include "../util/types.h"

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
  /**
   * @brief Get the total amount of physical RAM installed in the system.
   * @return A Result containing the total RAM in bytes (u64) on success,
   * or an error message (String) on failure.
   */
  fn GetMemInfo() -> Result<u64, String>;

  /**
   * @brief Gets metadata about the currently playing media.
   * @return A Result containing the media information (String) on success,
   * or an error code (NowPlayingError) on failure.
   */
  fn GetNowPlaying() -> Result<String, NowPlayingError>;

  /**
   * @brief Gets the "pretty" name of the operating system.
   * @details Examples: "Ubuntu 24.04.2 LTS", "Windows 11 Pro 24H2", "macOS 15 Sequoia".
   * @return A Result containing the OS version String on success,
   * or an error message (String) on failure.
   */
  fn GetOSVersion() -> Result<String, String>;

  /**
   * @brief Attempts to retrieve the desktop environment.
   * @details This is most relevant on Linux. May check environment variables (XDG_CURRENT_DESKTOP),
   * session files, or running processes. On Windows/macOS, it might return a
   * UI theme identifier (e.g., "Fluent", "Aqua") or None.
   * @return An Option containing the detected DE name String, or None if detection fails or is not applicable.
   */
  fn GetDesktopEnvironment() -> Option<String>;

  /**
   * @brief Attempts to retrieve the window manager.
   * @details On Linux, checks Wayland compositor or X11 WM properties. On Windows, returns "DWM" or similar.
   * On macOS, might return "Quartz Compositor" or a specific tiling WM name if active.
   * @return A String containing the detected WM name, or None if detection fails or is not applicable.
   */
  fn GetWindowManager() -> Option<String>;

  /**
   * @brief Attempts to detect the current user shell.
   * @details Checks the SHELL environment variable on Linux/macOS. On Windows, inspects the process tree
   * to identify known shells like PowerShell, Cmd, or MSYS2 shells (Bash, Zsh).
   * @return A String containing the detected shell name (e.g., "Bash", "Zsh", "PowerShell", "Fish").
   * May return the full path or "Unknown" as a fallback.
   */
  fn GetShell() -> String;

  /**
   * @brief Gets a system identifier, often the hardware model or product family.
   * @details Examples: "MacBookPro18,3", "Latitude 5420", "ThinkPad T490".
   * Implementation varies: reads DMI info on Linux, registry on Windows, sysctl on macOS.
   * @return A String containing the host/product identifier. May be empty if retrieval fails.
   */
  fn GetHost() -> String;

  /**
   * @brief Gets the operating system's kernel version string.
   * @details Examples: "5.15.0-76-generic", "10.0.22621", "23.1.0".
   * Uses uname() on Linux/macOS, WinRT/registry on Windows.
   * @return A String containing the kernel version. May be empty if retrieval fails.
   */
  fn GetKernelVersion() -> String;

  /**
   * @brief Gets the number of installed packages (Linux-specific).
   * @details Sums counts from various package managers (dpkg, rpm, pacman, flatpak, snap, etc.).
   * Returns 0 on non-Linux platforms or if no package managers are found.
   * @return A u64 representing the total count of detected packages.
   */
  fn GetPackageCount() -> u64; // Note: Implementation likely exists only in linux.cpp

  /**
   * @brief Gets the disk usage for the primary/root filesystem.
   * @details Uses statvfs on Linux/macOS, GetDiskFreeSpaceExW on Windows.
   * @return A Pair<u64, u64> where:
   * - first: Used disk space in bytes.
   * - second: Total disk space in bytes.
   * Returns {0, 0} on failure.
   */
  fn GetDiskUsage() -> Pair<u64, u64>;
}
