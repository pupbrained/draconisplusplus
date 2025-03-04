#pragma once

#include <expected>

#include "../util/macros.h"
#include "../util/types.h"

using std::optional, std::expected;

/**
 * @brief Get the amount of installed RAM in bytes.
 */
fn GetMemInfo() -> expected<u64, string>;

/**
 * @brief Get the currently playing song metadata.
 */
fn GetNowPlaying() -> expected<string, NowPlayingError>;

/**
 * @brief Get the OS version.
 */
fn GetOSVersion() -> expected<string, string>;

/**
 * @brief Get the current desktop environment.
 */
fn GetDesktopEnvironment() -> optional<string>;

/**
 * @brief Get the current window manager.
 */
fn GetWindowManager() -> string;

/**
 * @brief Get the current shell.
 */
fn GetShell() -> string;

/**
 * @brief Get the product family
 */
fn GetHost() -> string;

/**
 * @brief Get the kernel version.
 */
fn GetKernelVersion() -> string;

/**
 * @brief Get the number of installed packages.
 */
fn GetPackageCount() -> u64;

/**
 * @brief Get the current disk usage.
 * @return std::pair<u64, u64> Used space/total space
 */
fn GetDiskUsage() -> std::pair<u64, u64>;
