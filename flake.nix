{
  description = "C/C++ environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    utils.url = "github:numtide/flake-utils";
    devkitNix.url = "github:bandithedoge/devkitNix";
  };

  outputs = {
    self,
    nixpkgs,
    treefmt-nix,
    utils,
    devkitNix,
    ...
  }: let
    inherit (nixpkgs) lib;
  in
    {homeModules.default = import ./nix/module.nix {inherit self;};}
    // utils.lib.eachDefaultSystem (
      system: let
        isLinux = lib.strings.hasInfix "linux" system;

        pkgs = import nixpkgs {
          inherit system;
          overlays = lib.optionals isLinux [devkitNix.overlays.default];
        };

        llvmPackages = pkgs.llvmPackages_20;

        stdenv = with pkgs;
          (
            if hostPlatform.isLinux
            then stdenvAdapters.useMoldLinker
            else lib.id
          )
          llvmPackages.stdenv;

        devShellDeps = with pkgs;
          [
            ((glaze.override {
                enableAvx2 = hostPlatform.isx86;
              }).overrideAttrs rec {
                version = "5.5.4";

                src = fetchFromGitHub {
                  owner = "stephenberry";
                  repo = "glaze";
                  tag = "v${version}";
                  hash = "sha256-v6/IJlwc+nYgTAn8DJcbRC+qhZtUR6xu45dwm7rueV8=";
                };
              })
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
            asio
            curl
            libunistring
            magic-enum
            sqlitecpp
            (tomlplusplus.overrideAttrs {
              doCheck = false;
            })
          ])
          ++ darwinPkgs
          ++ linuxPkgs
          ++ lib.optionals isLinux (with pkgs.devkitNix; [
            devkitA64
          ]);

        darwinPkgs = lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic; [
          libiconv
          apple-sdk_15
        ]);

        linuxPkgs = lib.optionals stdenv.isLinux (with pkgs;
          [valgrind]
          ++ (with pkgsStatic; [
            dbus
            pugixml
            xorg.libxcb
            wayland
          ]));

        draconisPkgs = import ./nix ({inherit nixpkgs self system lib;} // lib.optionalAttrs isLinux {devkitNix = devkitNix;});
      in {
        packages = draconisPkgs;
        checks = draconisPkgs;

        devShells.default = pkgs.mkShell.override {inherit stdenv;} {
          packages =
            (with pkgs; [
              alejandra
              bear
              cachix
              cmake
              hyperfine
              llvmPackages.clang-tools
              meson
              ninja
              pkg-config

              (writeScriptBin "build" "meson compile -C build")
              (writeScriptBin "clean" ("meson setup build --wipe -Dprecompiled_config=true" + lib.optionalString pkgs.stdenv.isLinux " -Duse_linked_pci_ids=true"))
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
              vulkanDir = "${pkgs.mesa}/share/vulkan/icd.d";
              vulkanFiles = builtins.filter (file: builtins.match ".*\\.json$" file != null) (builtins.attrNames (builtins.readDir vulkanDir));
            in
              lib.concatStringsSep ":" (map (file: "${vulkanDir}/${file}") vulkanFiles);

          shellHook =
            lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
              export SDKROOT=${pkgs.pkgsStatic.apple-sdk_15}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
              export DEVELOPER_DIR=${pkgs.pkgsStatic.apple-sdk_15}
              export LDFLAGS="-L${pkgs.pkgsStatic.libiconvReal}/lib $LDFLAGS"
              export NIX_CFLAGS_COMPILE="-isysroot $SDKROOT"
              export NIX_CXXFLAGS_COMPILE="-isysroot $SDKROOT"
              export NIX_OBJCFLAGS_COMPILE="-isysroot $SDKROOT"
              export NIX_OBJCXXFLAGS_COMPILE="-isysroot $SDKROOT"
            ''
            + lib.optionalString pkgs.stdenv.hostPlatform.isLinux ''
              cp ${pkgs.pciutils}/share/pci.ids pci.ids
              chmod +w pci.ids
              objcopy -I binary -O default pci.ids pci_ids.o
              rm pci.ids
              export DEVKITPRO=${pkgs.devkitNix.devkitA64}/opt/devkitpro
              export PATH=$DEVKITPRO/devkitA64/bin:$PATH
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
