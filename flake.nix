{
  description = "C/C++ environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs";
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

        reflect-cpp = stdenv.mkDerivation {
          name = "reflect-cpp";
          version = "0.11.1";

          src = pkgs.fetchFromGitHub {
            owner = "getml";
            repo = "reflect-cpp";
            rev = "1ce78479ac9d04eb396ad972d656858eb06661d2";
            hash = "sha256-8TW2OlCbQZ07HypoYQE/wo29mxJWJwLziK1BpkhdFBo=";
          };

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

        linuxPkgs = nixpkgs.lib.optionals stdenv.isLinux (with pkgs.pkgsStatic; [
          glib
          systemdLibs
          sdbus-cpp
          valgrind
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
