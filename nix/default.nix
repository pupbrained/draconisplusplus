{
  nixpkgs,
  self,
  system,
  ...
}: let
  pkgs = import nixpkgs {inherit system;};

  dracPackages = import ./package.nix {inherit pkgs self;};

  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self;}
    else {};
in
  dracPackages
  // muslPackages
  // {default = dracPackages."generic";}
