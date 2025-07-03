/**
 * @file Bridge.hpp
 * @brief Provides a C++ bridge to Objective-C macOS frameworks.
 *
 * This file declares functions that wrap macOS-specific APIs, allowing them
 * to be called from C++ code. This is necessary for interacting with frameworks
 * like Metal and MediaRemote.
 */

#pragma once

#ifdef __APPLE__

  #include <Drac++/Utils/DataTypes.hpp>
  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace draconis::core::system::macOS {
  namespace {
    using draconis::utils::types::MediaInfo;
    using draconis::utils::types::Result;
    using draconis::utils::types::String;
  } // namespace

  /**
   * @brief Fetches the currently playing song's title and artist from macOS.
   * @return A Result containing a MediaInfo struct on success, or a DracError on failure.
   *
   * @note This function dynamically loads the private MediaRemote.framework to access
   * the MRMediaRemoteGetNowPlayingInfo function. This is an unsupported Apple API and
   * could break in future macOS updates.
   *
   * The retrieval is asynchronous, so this function uses a dispatch_semaphore to
   * block and wait for the result from the callback.
   */
  fn GetNowPlayingInfo() -> Result<MediaInfo>;

  /**
   * @brief Gets the model name of the primary system GPU.
   * @return A Result containing the GPU name as a String on success, or a DracError on failure.
   *
   * This function uses the Metal framework, which is Apple's modern graphics API,
   * to identify the default graphics device.
   */
  fn GetGPUModel() -> Result<String>;

  /**
   * @brief Gets the version of the macOS operating system.
   * @return A Result containing the version as a String on success, or a DracError on failure.
   *
   * @note When using Nix to build the draconis++ library, the Apple SDK available in nixpkgs
   * returns 16, while the actual Tahoe SDK returns 26. To get around that, we simply add 10
   * to the displayed major version.
   */
  fn GetOSVersion() -> Result<String>;
} // namespace draconis::core::system::macOS

#endif
