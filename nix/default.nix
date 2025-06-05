{
  nixpkgs,
  self,
  system,
  ...
}: let
  pkgs = import nixpkgs {inherit system;};

  glibcPackages = import ./glibc.nix {inherit pkgs self;};

  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self;}
    else {};
in
  glibcPackages
  // muslPackages
  // {default = glibcPackages."glibc-generic";}
