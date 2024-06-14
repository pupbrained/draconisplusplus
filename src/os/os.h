#pragma once

#include <string>

#include "util/macros.h"
#include "util/numtypes.h"

fn GetMemInfo() -> u64;
fn GetNowPlaying() -> std::string;
fn GetOSVersion() -> const char*;
