#pragma once

#ifdef __linux__

// clang-format off
#include <filesystem>            // std::filesystem::path
#include <glaze/core/common.hpp> // glz::object
#include <glaze/core/meta.hpp>   // glz::detail::Object

#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/types.hpp"
// clang-format on

namespace os::linux {
  using util::error::DracError;
  using util::types::Result, util::types::u64, util::types::i64, util::types::String;
  namespace fs = std::filesystem;

  struct PackageManagerInfo {
    String   id;
    fs::path dbPath;
    String   countQuery;
  };

  struct PkgCountCacheData {
    u64 count {};
    i64 timestampEpochSeconds {};

    // NOLINTBEGIN(readability-identifier-naming)
    struct [[maybe_unused]] glaze {
      using T = PkgCountCacheData;

      static constexpr glz::detail::Object value =
        glz::object("count", &T::count, "timestamp", &T::timestampEpochSeconds);
    };
    // NOLINTEND(readability-identifier-naming)
  };

  // Get package count from dpkg (Debian/Ubuntu)
  fn GetDpkgPackageCount() -> Result<u64, DracError>;

  // Get package count from RPM (Red Hat/Fedora/CentOS)
  fn GetRpmPackageCount() -> Result<u64, DracError>;

  // Get package count from pacman (Arch Linux)
  fn GetPacmanPackageCount() -> Result<u64, DracError>;

  // Get package count from Portage (Gentoo)
  fn GetPortagePackageCount() -> Result<u64, DracError>;

  // Get package count from zypper (openSUSE)
  fn GetZypperPackageCount() -> Result<u64, DracError>;

  // Get package count from apk (Alpine)
  fn GetApkPackageCount() -> Result<u64, DracError>;

  // Get package count from moss (AerynOS)
  fn GetMossPackageCount() -> Result<u64, DracError>;

  // Get package count from nix
  fn GetNixPackageCount() -> Result<u64, DracError>;

  // Get package count from flatpak
  fn GetFlatpakPackageCount() -> Result<u64, DracError>;

  // Get package count from snap
  fn GetSnapPackageCount() -> Result<u64, DracError>;

  // Get package count from AppImage
  fn GetAppimagePackageCount() -> Result<u64, DracError>;

  // Get total package count from all available package managers
  fn GetTotalPackageCount() -> Result<u64, DracError>;
} // namespace os::linux

#endif // __linux__
