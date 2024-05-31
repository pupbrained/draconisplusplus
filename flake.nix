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

        stdenv = pkgs.llvmPackages_18.stdenv;
      in
        with pkgs; {
          packages = rec {
            draconis-cpp = stdenv.mkDerivation {
              name = "draconis++";
              version = "0.1.0";
              src = self;

              nativeBuildInputs = [
                meson
                ninja
                pkg-config
              ];

              propagatedBuildInputs =
                [
                  libcpr
                  tomlplusplus
                ]
                ++ (lib.optionals pkgs.hostPlatform.isLinux [
                  glib
                  playerctl
                ]);

              buildInputs = [
                boost185
                fmt
              ];

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

            default = draconis-cpp;
          };

          formatter = alejandra;

          devShell = mkShell.override {inherit stdenv;} {
            packages =
              [
                alejandra
                bear
                clang-tools
                meson
                ninja
                pkg-config

                boost185
                fmt
                glib
                libcpr
                tomlplusplus
              ]
              ++ (lib.optionals pkgs.hostPlatform.isLinux [playerctl]);

            buildInputs = [
              libcpr
              tomlplusplus
            ];

            name = "C++";
          };
        }
    );
}
