#pragma once

#include <filesystem>

#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"

namespace os::linux {
  using util::error::DraconisError;
  using util::types::Result, util::types::u64;

  struct PackageManagerInfo {
    util::types::String   id;
    std::filesystem::path db_path;
    util::types::String   count_query;
  };

  // Get package count from dpkg (Debian/Ubuntu)
  fn GetDpkgPackageCount() -> Result<u64, DraconisError>;

  // Get package count from RPM (Red Hat/Fedora/CentOS)
  fn GetRpmPackageCount() -> Result<u64, DraconisError>;

  // Get package count from pacman (Arch Linux)
  fn GetPacmanPackageCount() -> Result<u64, DraconisError>;

  // Get package count from Portage (Gentoo)
  fn GetPortagePackageCount() -> Result<u64, DraconisError>;

  // Get package count from zypper (openSUSE)
  fn GetZypperPackageCount() -> Result<u64, DraconisError>;

  // Get package count from apk (Alpine)
  fn GetApkPackageCount() -> Result<u64, DraconisError>;

  // Get package count from moss (AerynOS)
  fn GetMossPackageCount() -> Result<u64, DraconisError>;

  // Get package count from nix
  fn GetNixPackageCount() -> Result<u64, DraconisError>;

  // Get package count from flatpak
  fn GetFlatpakPackageCount() -> Result<u64, DraconisError>;

  // Get package count from snap
  fn GetSnapPackageCount() -> Result<u64, DraconisError>;

  // Get package count from AppImage
  fn GetAppimagePackageCount() -> Result<u64, DraconisError>;

  // Get total package count from all available package managers
  fn GetTotalPackageCount() -> Result<u64, DraconisError>;
} // namespace os::linux