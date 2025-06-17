#pragma once

#ifdef __APPLE__

  #include <DracUtils/Definitions.hpp>
  #include <DracUtils/Error.hpp>
  #include <DracUtils/Types.hpp>

namespace os::bridge {
  fn GetNowPlayingInfo() -> drac::types::Result<drac::types::MediaInfo>;
  fn GetGPUModel() -> drac::types::Result<drac::types::String>;
} // namespace os::bridge

#endif
