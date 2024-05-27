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
        stdenv = pkgs.ccacheStdenv.override {stdenv = pkgs.stdenvAdapters.useMoldLinker pkgs.clangStdenv;};
      in {
        packages = rec {
          draconis-cpp = with pkgs;
            stdenv.mkDerivation {
              name = "draconis++";
              src = self;

              nativeBuildInputs = [
                cmake
                pkg-config
              ];

              propagatedBuildInputs = [
                glib
                playerctl
              ];

              buildInputs = [
                fmt
              ];

              buildPhase = ''
                cmake .
                make
              '';

              installPhase = ''
                install -Dm755 ./draconis++ $out/bin/draconis++
              '';
            };

          default = draconis-cpp;
        };

        devShell = with pkgs;
          mkShell.override {inherit stdenv;} {
            CXX = "${clang}/bin/clang++";
            CC = "${clang}/bin/clang++";

            packages = with pkgs; [
              # builder
              gnumake
              cmake
              pkg-config

              # debugger
              lldb

              # fix headers not found
              clang-tools

              # LSP and compiler
              llvm.libstdcxxClang

              # other tools
              cppcheck
              llvm.libllvm

              # stdlib for cpp
              llvm.libcxx

              # libraries
              fmt
              glib
              playerctl
              tomlplusplus
            ];

            name = "C++";
          };
      }
    );
}
