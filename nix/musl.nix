# nix/musl.nix
{
  pkgs,
  nixpkgs,
  self
}:
# system is x86_64-linux-musl
let
  # Import nixpkgs for x86_64-linux-musl
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

  # This stdenv is for the main draconisplusplus package
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

  # Helper to override packages to use the musl stdenv and build static libs
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

  # Dependencies for Musl builds
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

  # Common derivation function for Musl-based draconisplusplus
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
      src = self; # This `self` is the flake's source

      nativeBuildInputs = with muslPkgs; [
        cmake
        meson
        ninja
        pkg-config
      ];

      buildInputs = deps;

      configurePhase = ''
        meson setup build --buildtype release -Dbuild_for_musl=true
      '';

      buildPhase = ''
        meson compile -C build
      '';

      checkPhase = ''
        echo "Running tests..."
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
