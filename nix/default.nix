{
  nixpkgs,
  self,
  system,
  lib,
  ...
}: let
  pkgs = import nixpkgs {inherit system;};

  dracPackages = import ./package.nix {inherit pkgs self lib;};

  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self lib;}
    else {};
in
  dracPackages
  // muslPackages
  // {default = dracPackages."generic";}
