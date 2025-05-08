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
      system:
        if system == "x86_64-linux"
        then let
          pkgs = import nixpkgs {inherit system;};
          muslPkgs = import nixpkgs {
            system = "x86_64-linux-musl";
            overlays = [
              (self: super: {
                mimalloc = super.mimalloc.overrideAttrs (oldAttrs: {
                  cmakeFlags =
                    (oldAttrs.cmakeFlags or [])
                    ++ [(self.lib.cmakeBool "MI_LIBC_MUSL" true)];

                  postPatch = ''
                    sed -i '\|<linux/prctl.h>|s|^|// |' src/prim/unix/prim.c
                  '';
                });
              })
            ];
          };

          llvmPackages = muslPkgs.llvmPackages_20;

          stdenv =
            muslPkgs.stdenvAdapters.useMoldLinker
            llvmPackages.libcxxStdenv;

          glaze = (muslPkgs.glaze.override {inherit stdenv;}).overrideAttrs (oldAttrs: {
            cmakeFlags =
              (oldAttrs.cmakeFlags or [])
              ++ [
                "-Dglaze_DEVELOPER_MODE=OFF"
                "-Dglaze_BUILD_EXAMPLES=OFF"
              ];

            doCheck = false;

            enableAvx2 = stdenv.hostPlatform.isx86;
          });

          mkOverridden = buildSystem: pkg: ((pkg.override {inherit stdenv;}).overrideAttrs (oldAttrs: {
            "${buildSystem}Flags" =
              (oldAttrs."${buildSystem}Flags" or [])
              ++ (
                if buildSystem == "meson"
                then ["-Ddefault_library=static"]
                else if buildSystem == "cmake"
                then [
                  "-D${pkgs.lib.toUpper pkg.pname}_BUILD_EXAMPLES=OFF"
                  "-D${pkgs.lib.toUpper pkg.pname}_BUILD_TESTS=OFF"
                  "-DBUILD_SHARED_LIBS=OFF"
                ]
                else throw "Invalid build system: ${buildSystem}"
              );
          }));

          deps = with pkgs.pkgsStatic; [
            curlMinimal
            dbus
            glaze
            llvmPackages.libcxx
            openssl
            sqlite
            wayland
            xorg.libXau
            xorg.libXdmcp
            xorg.libxcb

            (mkOverridden "cmake" ftxui)
            (mkOverridden "cmake" pugixml)
            (mkOverridden "cmake" sqlitecpp)
            (mkOverridden "meson" tomlplusplus)
          ];
        in {
          packages = rec {
            draconisplusplus = stdenv.mkDerivation {
              name = "draconis++";
              version = "0.1.0";
              src = self;

              nativeBuildInputs = with muslPkgs; [
                cmake
                meson
                ninja
                pkg-config
              ];

              buildInputs = deps;

              configurePhase = ''
                meson setup build --buildtype release
              '';

              buildPhase = ''
                meson compile -C build
              '';

              installPhase = ''
                mkdir -p $out/bin
                mv build/draconis++ $out/bin/draconis++
              '';

              NIX_ENFORCE_NO_NATIVE = 0;
              meta.staticExecutable = true;
            };

            draconisplusplus-generic = draconisplusplus.overrideAttrs {NIX_ENFORCE_NO_NATIVE = 1;};

            default = draconisplusplus;
          };

          devShell = muslPkgs.mkShell.override {inherit stdenv;} {
            packages =
              (with pkgs; [bear cmake])
              ++ (with muslPkgs; [
                llvmPackages_20.clang-tools
                meson
                ninja
                pkg-config
                (pkgs.writeScriptBin "build" "meson compile -C build")
                (pkgs.writeScriptBin "clean" "meson setup build --wipe")
                (pkgs.writeScriptBin "run" "meson compile -C build && build/draconis++")
              ])
              ++ deps;

            NIX_ENFORCE_NO_NATIVE = 0;
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
        else let
          pkgs = import nixpkgs {inherit system;};

          llvmPackages = pkgs.llvmPackages_20;

          stdenv = with pkgs;
            (
              if hostPlatform.isLinux
              then stdenvAdapters.useMoldLinker
              else lib.id
            )
            llvmPackages.libcxxStdenv;

          deps = with pkgs;
            [
              (glaze.override {enableAvx2 = hostPlatform.isx86;})
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
            [
              valgrind
            ]
            ++ (with pkgsStatic; [
              dbus
              pugixml
              xorg.libxcb
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
                  meson setup build --buildtype release
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

            devShell = mkShell.override {inherit stdenv;} rec {
              packages =
                [
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
                ]
                ++ deps;

              LD_LIBRARY_PATH = "${lib.makeLibraryPath deps}";
              NIX_ENFORCE_NO_NATIVE = 0;
              SDKROOT = lib.optionalString stdenv.isDarwin "${pkgs.pkgsStatic.apple-sdk_15}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk";
              NIX_CFLAGS_COMPILE = lib.optionalString stdenv.isDarwin "-isysroot ${SDKROOT}";
              NIX_CXXFLAGS_COMPILE = lib.optionalString stdenv.isDarwin "-isysroot ${SDKROOT}";
              NIX_OBJCFLAGS_COMPILE = lib.optionalString stdenv.isDarwin "-isysroot ${SDKROOT}";
              NIX_OBJCXXFLAGS_COMPILE = lib.optionalString stdenv.isDarwin "-isysroot ${SDKROOT}";
            };
          }
    );
}
