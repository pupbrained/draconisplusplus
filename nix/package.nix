{
  pkgs,
  self,
  stringzilla-pkg,
  ...
}: let
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
    ++ (with pkgs.pkgsStatic; [
      curl
      ftxui
      gtest
      sqlitecpp
      (tomlplusplus.overrideAttrs {
        doCheck = false;
      })
    ])
    ++ darwinPkgs
    ++ linuxPkgs;

  darwinPkgs = pkgs.lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic; [
    libiconv
    apple-sdk_15
  ]);

  linuxPkgs = pkgs.lib.optionals stdenv.isLinux (with pkgs;
    [
      valgrind
    ]
    ++ (with pkgsStatic; [
      dbus
      pugixml
      stringzilla-pkg
      xorg.libxcb
      wayland
    ]));

  mkDraconisPackage = {native}:
    stdenv.mkDerivation {
      name =
        "draconis++"
        + (
          if native
          then "-native"
          else "-generic"
        );
      version = "0.1.0";
      src = self;

      nativeBuildInputs = with pkgs; [
        cmake
        meson
        ninja
        pkg-config
      ];

      buildInputs = deps;

      mesonFlags = [
        "-Dbuild_examples=false"
        "-Dbuild_tests=false"
      ];

      configurePhase = ''
        meson setup build --buildtype=release $mesonFlags
      '';

      buildPhase = ''
        meson compile -C build
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
    };
in {
  "generic" = mkDraconisPackage {native = false;};
  "native" = mkDraconisPackage {native = true;};
}
