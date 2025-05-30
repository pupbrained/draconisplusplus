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

        llvmPackages = pkgs.llvmPackages_20;

        stdenv = with pkgs;
          (
            if hostPlatform.isLinux
            then stdenvAdapters.useMoldLinker
            else lib.id
          )
          llvmPackages.stdenv;

        devShellDeps = with pkgs;
          [(glaze.override {enableAvx2 = hostPlatform.isx86;})]
          ++ (with pkgsStatic; [
            curl
            ftxui
            sqlitecpp
            (tomlplusplus.overrideAttrs {
              doCheck = false;
            })
          ])
          ++ darwinPkgs
          ++ linuxPkgs;

        darwinPkgs = nixpkgs.lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic; [
          libiconv
          apple-sdk_15
        ]);

        linuxPkgs = nixpkgs.lib.optionals stdenv.isLinux (with pkgs;
          [valgrind]
          ++ (with pkgsStatic; [
            dbus
            pugixml
            xorg.libxcb
            wayland
          ]));
      in {
        packages = import ./nix {inherit nixpkgs self system;};

        devShell = pkgs.mkShell.override {inherit stdenv;} {
          packages =
            (with pkgs; [
              alejandra
              bear
              llvmPackages.clang-tools
              cmake
              include-what-you-use
              lldb
              hyperfine
              meson
              ninja
              nvfetcher
              pkg-config
              unzip

              (writeScriptBin "build" "meson compile -C build")
              (writeScriptBin "clean" "meson setup build --wipe")
              (writeScriptBin "run" "meson compile -C build && build/draconis++")
            ])
            ++ devShellDeps;

          NIX_ENFORCE_NO_NATIVE = 0;

          shellHook = pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
            export SDKROOT=${pkgs.pkgsStatic.apple-sdk_15}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
            export DEVELOPER_DIR=${pkgs.pkgsStatic.apple-sdk_15}
            export NIX_CFLAGS_COMPILE="-isysroot $SDKROOT"
            export NIX_CXXFLAGS_COMPILE="-isysroot $SDKROOT"
            export NIX_OBJCFLAGS_COMPILE="-isysroot $SDKROOT"
            export NIX_OBJCXXFLAGS_COMPILE="-isysroot $SDKROOT"
          '';
        };

        formatter = treefmt-nix.lib.mkWrapper pkgs {
          projectRootFile = "flake.nix";
          programs = {
            alejandra.enable = true;
            deadnix.enable = true;
            clang-format = {
              enable = true;
              package = pkgs.llvmPackages.clang-tools;
            };
          };
        };
      }
    );
}
