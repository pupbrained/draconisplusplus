#pragma once

#ifdef __APPLE__

  #include <DracUtils/Definitions.hpp>
  #include <DracUtils/Error.hpp>
  #include <DracUtils/Types.hpp>

namespace draconis::core::system::macOS::bridge {
  namespace {
    using draconis::utils::types::MediaInfo;
    using draconis::utils::types::Result;
    using draconis::utils::types::String;
  } // namespace

  fn GetNowPlayingInfo() -> Result<MediaInfo>;
  fn GetGPUModel() -> Result<String>;
} // namespace draconis::core::system::macOS::bridge

#endif
