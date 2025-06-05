{
  pkgs,
  nixpkgs,
  self,
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

      nativeBuildInputs = with muslPkgs; [
        cmake
        meson
        ninja
        pkg-config
      ];

      mesonFlags = [
        "-Dbuild_for_musl=true"
      ];

      buildInputs = deps;

      configurePhase = ''
        meson setup build --buildtype=release $mesonFlags
      '';

      buildPhase = ''
        meson compile -C build
      '';

      checkPhase = ''
        meson test -C build --print-errorlogs
      '';

      doCheck = true;

      installPhase = ''
        mkdir -p $out/bin
        mv build/draconis++ $out/bin/draconis++
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
