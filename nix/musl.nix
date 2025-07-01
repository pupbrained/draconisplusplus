{
  pkgs,
  nixpkgs,
  self,
  lib,
  ...
}: let
  muslPkgs = import nixpkgs {
    system = "x86_64-linux-musl";
    overlays = [
      (final: prev: {
        mimalloc = prev.mimalloc.overrideAttrs (oldAttrs: {
          cmakeFlags =
            (oldAttrs.cmakeFlags or [])
            ++ [(final.lib.cmakeBool "MI_LIBC_MUSL" true)];

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

  glaze = (muslPkgs.glaze.override {inherit stdenv;}).overrideAttrs (oldAttrs: rec {
    version = "5.5.0";

    src = pkgs.fetchFromGitHub {
      owner = "stephenberry";
      repo = "glaze";
      tag = "v${version}";
      hash = "sha256-HC8R1wyNySVhuTZczdbiHkQ8STTXA/1GJLKdTXN9VAo=";
    };

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
          "-D${muslPkgs.lib.toUpper pkg.pname}_BUILD_EXAMPLES=OFF"
          "-D${muslPkgs.lib.toUpper pkg.pname}_BUILD_TESTS=OFF"
          "-DBUILD_SHARED_LIBS=OFF"
        ]
        else throw "Invalid build system: ${buildSystem}"
      );
  }));

  deps = with pkgs.pkgsStatic; [
    curlMinimal
    dbus
    glaze
    (mkOverridden "cmake" gtest)
    llvmPackages_20.libcxx
    magic-enum
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

  mkDraconisPackage = {native}:
    stdenv.mkDerivation {
      name =
        "draconis++-musl"
        + (
          if native
          then "-native"
          else "-generic"
        );
      version = "0.1.0";
      src = self;

      nativeBuildInputs = with pkgs;
        [
          cmake
          meson
          ninja
          pkg-config
        ]
        ++ lib.optional stdenv.isLinux xxd;

      mesonFlags = [
        "-Dbuild_for_musl=true"
        "-Dbuild_examples=false"
        "-Duse_linked_pci_ids=true"
      ];

      buildInputs = deps;

      configurePhase = ''
        meson setup build --buildtype=release $mesonFlags
      '';

      buildPhase = ''
        cp ${pkgs.pciutils}/share/pci.ids pci.ids
        chmod +w pci.ids
        objcopy -I binary -O default pci.ids pci_ids.o
        rm pci.ids

        export LDFLAGS="$LDFLAGS $PWD/pci_ids.o"

        meson compile -C build
      '';

      checkPhase = ''
        meson test -C build --print-errorlogs
      '';

      installPhase = ''
        mkdir -p $out/bin
        mv build/src/CLI/draconis++ $out/bin/draconis++
      '';

      NIX_ENFORCE_NO_NATIVE =
        if native
        then 0
        else 1;

      meta.staticExecutable = true;
    };
in {
  "musl-generic" = mkDraconisPackage {native = false;};
  "musl-native" = mkDraconisPackage {native = true;};
}
