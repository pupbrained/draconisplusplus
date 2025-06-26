{
  pkgs,
  self,
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
      ((glaze.override {enableAvx2 = hostPlatform.isx86;}).overrideAttrs rec {
        version = "5.5.0";

        src = pkgs.fetchFromGitHub {
          owner = "stephenberry";
          repo = "glaze";
          tag = "v${version}";
          hash = "sha256-HC8R1wyNySVhuTZczdbiHkQ8STTXA/1GJLKdTXN9VAo=";
        };
      })
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

      nativeBuildInputs = with pkgs; ([
          cmake
          meson
          ninja
          pkg-config
        ]
        ++ pkgs.lib.optional stdenv.isLinux xxd);

      buildInputs = deps;

      mesonFlags = [
        "-Dbuild_examples=false"
        (pkgs.lib.optionalString stdenv.isLinux "-Duse_linked_pci_ids=true")
      ];

      configurePhase = ''
        meson setup build --buildtype=release $mesonFlags
      '';

      buildPhase =
        pkgs.lib.optionalString stdenv.isLinux ''
          cp ${pkgs.pciutils}/share/pci.ids pci.ids
          chmod +w pci.ids
          objcopy -I binary -O default pci.ids pci_ids.o
          rm pci.ids

          export LDFLAGS="$LDFLAGS $PWD/pci_ids.o"
        ''
        + ''
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
    };
in {
  "generic" = mkDraconisPackage {native = false;};
  "native" = mkDraconisPackage {native = true;};
}
