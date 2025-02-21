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

        llvmPackages = with pkgs;
          if hostPlatform.isLinux
          then llvmPackages_20
          else llvmPackages_19;

        stdenv = with pkgs;
          (
            if hostPlatform.isLinux
            then stdenvAdapters.useMoldLinker
            else lib.id
          )
          llvmPackages.stdenv;

        sources = import ./_sources/generated.nix {
          inherit (pkgs) fetchFromGitHub fetchgit fetchurl dockerTools;
        };

        mkPkg = name:
          pkgs.pkgsStatic.${name}.overrideAttrs {
            inherit (sources.${name}) pname version src;
          };

        fmt = mkPkg "fmt";

        tomlplusplus = pkgs.pkgsStatic.tomlplusplus.overrideAttrs {
          inherit (sources.tomlplusplus) pname version src;
          doCheck = false;
        };

        sdbus-cpp = pkgs.sdbus-cpp.overrideAttrs {
          inherit (sources.sdbus-cpp) pname version src;

          cmakeFlags = [
            (pkgs.lib.cmakeBool "BUILD_CODE_GEN" true)
            (pkgs.lib.cmakeBool "BUILD_SHARED_LIBS" false)
          ];
        };

        yyjson = pkgs.pkgsStatic.stdenv.mkDerivation {
          inherit (sources.yyjson) pname version src;

          nativeBuildInputs = with pkgs; [cmake ninja pkg-config];
        };

        reflect-cpp = stdenv.mkDerivation rec {
          inherit (sources.reflect-cpp) pname version src;

          buildInputs = [tomlplusplus yyjson];
          nativeBuildInputs = buildInputs ++ (with pkgs; [cmake ninja pkg-config]);

          cmakeFlags = [
            "-DCMAKE_TOOLCHAIN_FILE=OFF"
            "-DREFLECTCPP_TOML=ON"
            "-DREFLECTCPP_JSON=ON"
            "-DREFLECTCPP_USE_STD_EXPECTED=ON"
          ];
        };

        deps = with pkgs.pkgsStatic;
          [
            # curl
            # fmt
            # libiconv
            # tomlplusplus
            # yyjson
            # reflect-cpp
            sqlitecpp
            # ftxui
          ]
          ++ linuxPkgs;

        linuxPkgs = nixpkgs.lib.optionals stdenv.isLinux (with pkgs;
          [
            systemdLibs
            valgrind
          ]
          ++ (with pkgsStatic; [
            glib
            sdbus-cpp
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

            name = "C++";
          };
        }
    );
}
