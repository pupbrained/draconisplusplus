#pragma once

#ifdef __APPLE__

// clang-format off
#include "DracUtils/Definitions.hpp"
#include "DracUtils/Error.hpp"
#include "DracUtils/Types.hpp"
// clang-format on

namespace os::bridge {
  fn GetNowPlayingInfo() -> util::types::Result<util::types::MediaInfo>;
  fn GetGPUModel() -> util::types::Result<util::types::SZString>;
}

#endif
