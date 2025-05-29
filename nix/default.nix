# nix/default.nix
{
  nixpkgs, # The top-level nixpkgs input from the flake
  self, # The flake itself
  system, # The current system being evaluated for
  ... # Allow other inputs to be passed if necessary in the future
}: let
  # Instantiate pkgs for the current system (primarily for glibc builds)
  pkgs = import nixpkgs {inherit system;};

  # Glibc packages for the current system
  glibcPackages = import ./glibc.nix {inherit pkgs self;}; # Pass system-specific pkgs

  # Musl packages are always for x86_64-linux-musl
  # We only build these if the current system is also Linux, to avoid trying to cross-compile from macOS to musl for example
  # or if you specifically want to evaluate them.
  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self;} # Pass the top-level nixpkgs and self
    else {}; # Empty set if not on Linux, so attributes are not found
in
  # Combine all package sets
  glibcPackages // muslPackages
