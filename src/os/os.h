#pragma once

#include "../util/macros.h"
#include "../util/types.h"

namespace os {
  /**
   * @brief Get the amount of installed RAM in bytes.
   */
  fn GetMemInfo() -> Result<u64, String>;

  /**
   * @brief Get the currently playing song metadata.
   */
  fn GetNowPlaying() -> Result<String, NowPlayingError>;

  /**
   * @brief Get the OS version.
   */
  fn GetOSVersion() -> Result<String, String>;

  /**
   * @brief Get the current desktop environment.
   */
  fn GetDesktopEnvironment() -> Option<String>;

  /**
   * @brief Get the current window manager.
   */
  fn GetWindowManager() -> String;

  /**
   * @brief Get the current shell.
   */
  fn GetShell() -> String;

  /**
   * @brief Get the product family
   */
  fn GetHost() -> String;

  /**
   * @brief Get the kernel version.
   */
  fn GetKernelVersion() -> String;

  /**
   * @brief Get the number of installed packages.
   */
  fn GetPackageCount() -> u64;

  /**
   * @brief Get the current disk usage.
   * @return std::pair<u64, u64> Used space/total space
   */
  fn GetDiskUsage() -> Pair<u64, u64>;
}