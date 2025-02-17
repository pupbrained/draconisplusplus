#pragma once

// Get package count from dpkg (Debian/Ubuntu)
int get_dpkg_package_count();

// Get package count from RPM (Red Hat/Fedora/CentOS)
int get_rpm_package_count();

// Get package count from pacman (Arch Linux)
int get_pacman_package_count();

// Get package count from Portage (Gentoo)
int get_portage_package_count();

// Get package count from zypper (openSUSE)
int get_zypper_package_count();

// Get package count from flatpak
int get_flatpak_package_count();

// Get package count from snap
int get_snap_package_count();

// Get package count from AppImage
int get_appimage_package_count();

// Get total package count from all available package managers
fn GetTotalPackageCount() -> int;
