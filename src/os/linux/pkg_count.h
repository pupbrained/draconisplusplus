#pragma once

#include "src/util/macros.h"

// Get package count from dpkg (Debian/Ubuntu)
fn GetDpkgPackageCount() -> std::optional<usize>;

// Get package count from RPM (Red Hat/Fedora/CentOS)
fn GetRpmPackageCount() -> std::optional<usize>;

// Get package count from pacman (Arch Linux)
fn GetPacmanPackageCount() -> std::optional<usize>;

// Get package count from Portage (Gentoo)
fn GetPortagePackageCount() -> std::optional<usize>;

// Get package count from zypper (openSUSE)
fn GetZypperPackageCount() -> std::optional<usize>;

// Get package count from apk (Alpine)
fn GetApkPackageCount() -> std::optional<usize>;

// Get package count from flatpak
fn GetFlatpakPackageCount() -> std::optional<usize>;

// Get package count from snap
fn GetSnapPackageCount() -> std::optional<usize>;

// Get package count from AppImage
fn GetAppimagePackageCount() -> std::optional<usize>;

// Get total package count from all available package managers
fn GetTotalPackageCount() -> std::optional<usize>;
