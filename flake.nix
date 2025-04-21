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

        deps = with pkgs;
          [
            (glaze.override {enableAvx2 = hostPlatform.isx86;})
          ]
          ++ (with pkgsStatic; [
            curl
            ftxui
            tomlplusplus
          ])
          ++ darwinPkgs
          ++ linuxPkgs;

        darwinPkgs = nixpkgs.lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic; [libiconv]);

        linuxPkgs = nixpkgs.lib.optionals stdenv.isLinux (with pkgs;
          [
            valgrind
          ]
          ++ (with pkgsStatic; [
            dbus
            sqlitecpp
            xorg.libX11
            wayland
          ]));
      in
        with pkgs; {
          packages = rec {
            draconisplusplus = stdenv.mkDerivation {
              name = "draconis++";
              version = "0.1.0";
              src = self;

              nativeBuildInputs = [
                cmake
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
                package = pkgs.llvmPackages.clang-tools;
              };
            };
          };

          devShell = mkShell.override {inherit stdenv;} {
            packages =
              [
                alejandra
                bear
                llvmPackages.clang-tools
                cmake
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
              ]
              ++ deps;

            LD_LIBRARY_PATH = "${lib.makeLibraryPath deps}";
            NIX_ENFORCE_NO_NATIVE = 0;

            name = "C++";
          };
        }
    );
}
