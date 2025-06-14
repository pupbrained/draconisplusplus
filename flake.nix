{
  description = "C/C++ environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    stringzilla.url = "github:ashvardanian/StringZilla";
    stringzilla.flake = false;
    treefmt-nix.url = "github:numtide/treefmt-nix";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    stringzilla,
    treefmt-nix,
    utils,
    ...
  }:
    {homeModules.default = import ./nix/module.nix {inherit self;};}
    // utils.lib.eachDefaultSystem (
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

        stringzilla-pkg = stdenv.mkDerivation rec {
          pname = "stringzilla";
          version = "3.12.5";
          src = stringzilla;

          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];

          postPatch = ''
            sed -i '1i#include <type_traits>' include/stringzilla/stringzilla.hpp

            ${pkgs.lib.optionalString stdenv.isDarwin ''
              substituteInPlace CMakeLists.txt \
                --replace 'target_link_options(stringzillite PRIVATE "$<$<CXX_COMPILER_ID:GNU,Clang>:-nostdlib>")' "" \
                --replace '";-nostdlib>"' '";>"'
            ''}

            sed -i '/install(DIRECTORY .\/c\/ DESTINATION \/usr\/src/d' CMakeLists.txt
          '';

          NIX_CFLAGS_COMPILE = pkgs.lib.optionalString stdenv.isAarch64 "-fno-stack-protector";

          cmakeFlags = [
            "-DSTRINGZILLA_INSTALL=ON"
            "-DSTRINGZILLA_BUILD_TEST=OFF"
            "-DSTRINGZILLA_BUILD_BENCHMARK=OFF"
            "-DSTRINGZILLA_BUILD_SHARED=ON"
            "-DCMAKE_INSTALL_INCLUDEDIR=include"
            "-DCMAKE_INSTALL_LIBDIR=lib"
          ];

          postInstall = ''
            mkdir -p $out/lib/pkgconfig
            substituteAll ${./nix/stringzilla.pc.in} $out/lib/pkgconfig/stringzilla.pc
          '';
        };

        devShellDeps = with pkgs;
          [
            stringzilla-pkg
            (glaze.override {enableAvx2 = hostPlatform.isx86;})
            (imgui.override {
              IMGUI_BUILD_GLFW_BINDING = true;
              IMGUI_BUILD_VULKAN_BINDING = true;
            })
            gtest
            vulkan-extension-layer
            vulkan-memory-allocator
            vulkan-utility-libraries
            vulkan-loader
            vulkan-tools
          ]
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

        draconisPkgs = import ./nix {inherit nixpkgs self system;};
      in {
        packages = draconisPkgs;
        checks = draconisPkgs;

        devShells.default = pkgs.mkShell.override {inherit stdenv;} {
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

          VULKAN_SDK = "${pkgs.vulkan-headers}";
          VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
          VK_ICD_FILENAMES =
            if stdenv.isDarwin
            then "${pkgs.darwin.moltenvk}/share/vulkan/icd.d/MoltenVK_icd.json"
            else let
              vulkanDir = "${pkgs.mesa.drivers}/share/vulkan/icd.d";
              vulkanFiles = builtins.filter (file: builtins.match ".*\\.json$" file != null) (builtins.attrNames (builtins.readDir vulkanDir));
              vulkanPaths = nixpkgs.lib.concatStringsSep ":" (map (file: "${vulkanDir}/${file}") vulkanFiles);
            in
              if stdenv.hostPlatform.isx86_64
              then "${pkgs.linuxPackages_latest.nvidia_x11_beta}/share/vulkan/icd.d/nvidia_icd.x86_64.json:${vulkanPaths}"
              else vulkanPaths;

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
