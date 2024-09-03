{
  description = "C/C++ environment";

  inputs = {
    nixvim.url = "github:pupbrained/nvim-config";
    nixpkgs.url = "github:NixOS/nixpkgs";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    treefmt-nix,
    utils,
    nixvim,
    ...
  }:
    utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {inherit system;};

        stdenv =
          if pkgs.hostPlatform.isLinux
          then pkgs.stdenvAdapters.useMoldLinker pkgs.llvmPackages_18.stdenv
          else pkgs.llvmPackages_18.stdenv;

        sources = import ./_sources/generated.nix {
          inherit (pkgs) fetchFromGitHub fetchgit fetchurl dockerTools;
        };

        mkPkg = name:
          pkgs.pkgsStatic.${name}.overrideAttrs {
            inherit (sources.${name}) pname version src;
          };

        fmt = mkPkg "fmt";
        tomlplusplus = mkPkg "tomlplusplus";
        yyjson = mkPkg "yyjson";

        sdbus-cpp = pkgs.sdbus-cpp.overrideAttrs {
          inherit (sources.sdbus-cpp) pname version src;
        };

        reflect-cpp = stdenv.mkDerivation {
          inherit (sources.reflect-cpp) pname version src;

          nativeBuildInputs = with pkgs; [cmake ninja pkg-config];

          cmakeFlags = [
            "-DCMAKE_TOOLCHAIN_FILE=OFF"
            "-DCMAKE_BUILD_TYPE=Release"
            "-DREFLECTCPP_TOML=ON"
          ];
        };

        deps = with pkgs.pkgsStatic;
          [
            curl
            fmt
            libiconv
            tomlplusplus
            yyjson
            reflect-cpp
          ]
          ++ linuxPkgs
          ++ darwinPkgs;

        linuxPkgs = nixpkgs.lib.optionals stdenv.isLinux (with pkgs; [
          pkgsStatic.glib
          systemdLibs
          sdbus-cpp
          valgrind
          xorg.libX11
        ]);

        darwinPkgs = nixpkgs.lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic.darwin.apple_sdk.frameworks; [
          Foundation
          MediaPlayer
          SystemConfiguration
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
                nvfetcher
                pkg-config
                unzip
                nixvim.packages.${system}.default

                (writeScriptBin "build" "meson compile -C build")
                (writeScriptBin "clean" "meson setup build --wipe")
                (writeScriptBin "run" "meson compile -C build && build/draconis++")
              ]
              ++ deps;

            INCLUDE_DIR = "${llvmPackages_18.libcxx.dev}/include/c++/v1";

            name = "C++";
          };
        }
    );
}
