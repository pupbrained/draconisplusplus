{
  description = "C/C++ environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    treefmt-nix,
    utils,
    ...
  }:
    utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {inherit system;};

        stdenv =
          if pkgs.hostPlatform.isLinux
          then pkgs.stdenvAdapters.useMoldLinker pkgs.llvmPackages_18.stdenv
          else pkgs.llvmPackages_18.stdenv;

        deps = with (
          if !stdenv.isDarwin
          then pkgs.pkgsStatic
          else pkgs # TODO: Remove when fixed on darwin
        );
          [
            curl
            fmt
            glib
            tomlplusplus
            yyjson
          ]
          ++ linuxPkgs
          ++ darwinPkgs;

        linuxPkgs = nixpkgs.lib.optionals stdenv.isLinux (with pkgs; [
          systemdLibs
          sdbus-cpp
          valgrind
        ]);

        darwinPkgs = nixpkgs.lib.optionals stdenv.isDarwin (with pkgs.darwin.apple_sdk.frameworks; [
          Foundation
          MediaPlayer
        ]);
      in
        with pkgs; {
          packages = rec {
            draconisplusplus = stdenv.mkDerivation {
              name = "draconis++";
              version = "0.1.0";
              src = self;

              nativeBuildInputs = [
                meson
                ninja
                pkg-config
              ];

              buildInputs = deps;

              configurePhase = ''
                meson setup build
              '';

              buildPhase = ''
                meson compile -C build
              '';

              installPhase = ''
                mkdir -p $out/bin
                mv build/draconis++ $out/bin/draconis++
              '';
            };

            default = draconisplusplus;
          };

          formatter = treefmt-nix.lib.mkWrapper pkgs {
            projectRootFile = "flake.nix";
            programs = {
              alejandra.enable = true;
              deadnix.enable = true;

              clang-format = {
                enable = true;
                package = pkgs.clang-tools_18;
              };
            };
          };

          devShell = mkShell.override {inherit stdenv;} {
            packages =
              [
                alejandra
                bear
                clang-tools_18
                lldb
                meson
                ninja
                pkg-config
                unzip

                (writeScriptBin "build" "meson compile -C build")
                (writeScriptBin "clean" "meson setup build --wipe")
                (writeScriptBin "run" "meson compile -C build && build/draconis++")
              ]
              ++ deps;

            name = "C++";
          };
        }
    );
}
