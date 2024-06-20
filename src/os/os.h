#pragma once

#include <string>

#include "../util/macros.h"
#include "../util/numtypes.h"

/**
 * @brief Get the amount of installed RAM in bytes.
 */
fn GetMemInfo() -> u64;

/**
 * @brief Get the currently playing song metadata.
 */
fn GetNowPlaying() -> std::string;

/**
 * @brief Get the OS version.
 */
fn GetOSVersion() -> const char*;
