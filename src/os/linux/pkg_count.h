#pragma once

#include "src/util/macros.h"

// Get package count from dpkg (Debian/Ubuntu)
fn GetDpkgPackageCount() -> Option<usize>;

// Get package count from RPM (Red Hat/Fedora/CentOS)
fn GetRpmPackageCount() -> Option<usize>;

// Get package count from pacman (Arch Linux)
fn GetPacmanPackageCount() -> Option<usize>;

// Get package count from Portage (Gentoo)
fn GetPortagePackageCount() -> Option<usize>;

// Get package count from zypper (openSUSE)
fn GetZypperPackageCount() -> Option<usize>;

// Get package count from apk (Alpine)
fn GetApkPackageCount() -> Option<usize>;

// Get package count from nix
fn GetNixPackageCount() -> Option<usize>;

// Get package count from flatpak
fn GetFlatpakPackageCount() -> Option<usize>;

// Get package count from snap
fn GetSnapPackageCount() -> Option<usize>;

// Get package count from AppImage
fn GetAppimagePackageCount() -> Option<usize>;

// Get total package count from all available package managers
fn GetTotalPackageCount() -> Option<usize>;
