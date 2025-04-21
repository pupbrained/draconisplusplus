#pragma once

#include <expected>

#include "../util/macros.h"
#include "../util/types.h"

using std::optional, std::expected;

/**
 * @brief Get the amount of installed RAM in bytes.
 */
fn GetMemInfo() -> expected<u64, String>;

/**
 * @brief Get the currently playing song metadata.
 */
fn GetNowPlaying() -> expected<String, NowPlayingError>;

/**
 * @brief Get the OS version.
 */
fn GetOSVersion() -> expected<String, String>;

/**
 * @brief Get the current desktop environment.
 */
fn GetDesktopEnvironment() -> optional<String>;

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
fn GetDiskUsage() -> std::pair<u64, u64>;
