# nix/glibc.nix
{
  pkgs,
  self,
}:
# system is already incorporated into pkgs by the time it's passed here
let
  llvmPackages = pkgs.llvmPackages_20;

  stdenv = with pkgs;
    (
      if hostPlatform.isLinux
      then stdenvAdapters.useMoldLinker
      else lib.id
    )
    llvmPackages.stdenv;

  # Dependencies for Glibc builds, derived from the original flake's Glibc branch
  deps = with pkgs;
    [
      (glaze.override {enableAvx2 = hostPlatform.isx86;})
    ]
    ++ (with pkgs.pkgsStatic; [
      # Assuming pkgsStatic is desired for these
      curl
      ftxui
      gtest
      sqlitecpp
      (tomlplusplus.overrideAttrs {
        doCheck = false; # as per original
      })
    ])
    ++ darwinPkgs
    ++ linuxPkgs;

  darwinPkgs = pkgs.lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic; [
    libiconv
    apple-sdk_15 # Ensure this matches what devShell uses if consistency is needed
  ]);

  linuxPkgs = pkgs.lib.optionals stdenv.isLinux (with pkgs;
    [
      valgrind # Note: valgrind is not static, kept from original deps
    ]
    ++ (with pkgsStatic; [
      # Assuming pkgsStatic is desired
      dbus
      pugixml
      xorg.libxcb
      wayland
    ]));

  # Common derivation function for Glibc-based draconisplusplus
  mkDraconisPackage = {native}:
    stdenv.mkDerivation {
      name =
        "draconis++-glibc"
        + (
          if native
          then "-native"
          else "-generic"
        );
      version = "0.1.0";
      src = self; # This `self` is the flake's source

      nativeBuildInputs = with pkgs; [
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

      checkPhase = ''
        echo "Running tests..."
        meson test -C build --print-errorlogs
      '';

      doCheck = true;

      installPhase = ''
        mkdir -p $out/bin
        mv build/draconis++ $out/bin/draconis++
      '';

      # NIX_ENFORCE_NO_NATIVE = 1 for generic, 0 for native (specific)
      NIX_ENFORCE_NO_NATIVE =
        if native
        then 0
        else 1;
      # meta.staticExecutable for glibc is typically false unless LTO and other measures are taken.
    };
in {
  "glibc-generic" = mkDraconisPackage {native = false;};
  "glibc-native" = mkDraconisPackage {native = true;};
}

