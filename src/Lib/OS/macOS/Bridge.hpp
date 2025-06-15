#pragma once

#ifdef __APPLE__

  #include <DracUtils/Definitions.hpp>
  #include <DracUtils/Error.hpp>
  #include <DracUtils/Types.hpp>

namespace os::bridge {
  fn GetNowPlayingInfo() -> util::types::Result<util::types::MediaInfo>;
  fn GetGPUModel() -> util::types::Result<util::types::SZString>;
} // namespace os::bridge

#endif
