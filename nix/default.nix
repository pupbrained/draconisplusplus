{
  nixpkgs,
  self,
  system,
  stringzilla-pkg,
  ...
}: let
  pkgs = import nixpkgs {inherit system;};

  dracPackages = import ./package.nix {inherit pkgs self stringzilla-pkg;};

  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self stringzilla-pkg;}
    else {};
in
  dracPackages
  // muslPackages
  // {default = dracPackages."generic";}
