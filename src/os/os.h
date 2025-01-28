#pragma once

#include "../util/macros.h"
#include "../util/types.h"

/**
 * @brief Get the amount of installed RAM in bytes.
 */
fn GetMemInfo() -> u64;

/**
 * @brief Get the currently playing song metadata.
 */
fn GetNowPlaying() -> string;

/**
 * @brief Get the OS version.
 */
fn GetOSVersion() -> string;

/**
 * @brief Get the current desktop environment.
 */
fn GetDesktopEnvironment() -> string;

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
fn GetProductFamily() -> string;
