{
  nixpkgs,
  self,
  system,
  lib,
  # devkitNix ? null,
  ...
}: let
  pkgs = import nixpkgs {
    inherit system;
    # overlays = lib.optional (devkitNix != null) devkitNix.overlays.default;
  };

  dracPackages = import ./package.nix {inherit pkgs self lib;};

  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self lib;}
    else {};
in
  dracPackages
  // muslPackages
  // {default = dracPackages."generic";}
