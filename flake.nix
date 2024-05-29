{
  description = "C/C++ environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    utils,
    ...
  }:
    utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
          overlays = [
            (self: super: {
              ccacheWrapper = super.ccacheWrapper.override {
                extraConfig = ''
                  export CCACHE_COMPRESS=1
                  export CCACHE_DIR="/var/cache/ccache"
                  export CCACHE_UMASK=007
                  if [ ! -d "$CCACHE_DIR" ]; then
                    echo "====="
                    echo "Directory '$CCACHE_DIR' does not exist"
                    echo "Please create it with:"
                    echo "  sudo mkdir -m0770 '$CCACHE_DIR'"
                    echo "  sudo chown root:nixbld '$CCACHE_DIR'"
                    echo "====="
                    exit 1
                  fi
                  if [ ! -w "$CCACHE_DIR" ]; then
                    echo "====="
                    echo "Directory '$CCACHE_DIR' is not accessible for user $(whoami)"
                    echo "Please verify its access permissions"
                    echo "====="
                    exit 1
                  fi
                '';
              };
            })
          ];
        };

        llvm = pkgs.llvmPackages_latest;

        stdenv = pkgs.ccacheStdenv.override {
          stdenv =
            if pkgs.hostPlatform.isDarwin
            then llvm.libcxxStdenv
            else pkgs.stdenvAdapters.useMoldLinker pkgs.clangStdenv;
        };

        darwinPkgs = nixpkgs.lib.optionals pkgs.stdenv.isDarwin (with pkgs.darwin; [
          apple_sdk.frameworks.AppKit
          apple_sdk.frameworks.Carbon
          apple_sdk.frameworks.Cocoa
          apple_sdk.frameworks.CoreFoundation
          apple_sdk.frameworks.IOKit
          apple_sdk.frameworks.WebKit
          apple_sdk.frameworks.Security
          apple_sdk.frameworks.DisplayServices
        ]);
      in {
        packages = rec {
          draconis-cpp = with pkgs;
            stdenv.mkDerivation {
              name = "draconis++";
              src = self;

              nativeBuildInputs = [
                cmake
                ninja
                pkg-config
              ];

              propagatedBuildInputs =
                [
                  boost185
                  glib
                ]
                ++ (
                  if pkgs.hostPlatform.isLinux
                  then [playerctl]
                  else []
                );

              buildInputs =
                [
                  fmt
                  libcpr
                  tomlplusplus
                ]
                ++ darwinPkgs;

              buildPhase = ''
                cmake -GNinja .
                ninja
              '';

              installPhase = ''
                install -Dm755 ./draconis++ $out/bin/draconis++
              '';
            };

          default = draconis-cpp;
        };

        devShell = with pkgs;
          mkShell.override {inherit stdenv;} {
            packages = with pkgs;
              [
                # builder
                cmake
                ninja
                pkg-config

                # debugger
                lldb

                # fix headers not found
                clang-tools_18

                # LSP and compiler
                llvm.libstdcxxClang

                # other tools
                cppcheck
                llvm.libllvm

                # stdlib for cpp
                llvm.libcxx

                # libraries
                boost185
                fmt
                glib
                libcpr
                tomlplusplus
              ]
              ++ (
                if stdenv.isDarwin
                then []
                else [playerctl]
              )
              ++ darwinPkgs;

            name = "C++";
          };
      }
    );
}
