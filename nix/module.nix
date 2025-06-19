{self}: {
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.draconisplusplus;

  tomlFormat = pkgs.formats.toml {};

  defaultPackage = self.packages.${pkgs.system}.default;

  apiKey =
    if cfg.weatherApiKey == null
    then "std::nullopt"
    else cfg.weatherApiKey;

  location =
    if isAttrs cfg.location
    then
      # cpp
      "weather::Coords { .lat = ${toString cfg.location.lat}, .lon = ${toString cfg.location.lon} }"
    else "${cfg.location}";

  configHpp =
    pkgs.writeText "config.hpp"
    # cpp
    ''
      #pragma once

      #if DRAC_PRECOMPILED_CONFIG

      #if DRAC_ENABLE_WEATHER
        #include <Drac++/Services/Weather.hpp>
      #endif

      #if DRAC_ENABLE_PACKAGECOUNT
        #include <Drac++/Services/Packages.hpp>
      #endif

      namespace draconis::config {
        constexpr const char* DRAC_USERNAME = "${cfg.username}";

        #if DRAC_ENABLE_WEATHER
        constexpr services::weather::Provider DRAC_WEATHER_PROVIDER = services::weather::Provider::${lib.toUpper cfg.weatherProvider};
        constexpr services::weather::Unit DRAC_WEATHER_UNIT = services::weather::Unit::${lib.toUpper cfg.weatherUnit};
        constexpr bool DRAC_SHOW_TOWN_NAME = ${toString cfg.showTownName};
        constexpr std::optional<std::string> DRAC_API_KEY = ${apiKey};
        constexpr services::weather::Location DRAC_LOCATION = ${location};
        #endif

        #if DRAC_ENABLE_PACKAGECOUNT
        constexpr services::packages::Manager DRAC_ENABLED_PACKAGE_MANAGERS = ${builtins.concatStringsSep " | " (map (pkg: "services::packages::Manager::" + lib.toUpper pkg) cfg.packageManagers)};
        #endif
      }

      #endif
    '';

  draconisWithOverrides = cfg.package.overrideAttrs (oldAttrs: {
    postPatch = ''
      cp ${configHpp} ./config.hpp
    '';

    mesonFlags =
      (oldAttrs.mesonFlags or [])
      ++ [
        (lib.optionalString (cfg.configFormat == "hpp") "-Dprecompiled_config=true")
        (lib.optionalString (cfg.usePugixml) "-Duse_pugixml=true")
        (lib.optionalString (cfg.enableNowPlaying) "-Denable_nowplaying=true")
        (lib.optionalString (cfg.enableWeather) "-Denable_weather=true")
        (lib.optionalString (cfg.enablePackageCount) "-Denable_packagecount=true")
      ];
  });

  draconisPkg = draconisWithOverrides;
in {
  options.programs.draconisplusplus = {
    enable = mkEnableOption "draconis++";

    package = mkOption {
      type = types.package;
      default = defaultPackage;
      description = "The base draconis++ package.";
    };

    configFormat = mkOption {
      type = types.enum ["toml" "hpp"];
      default = "toml";
      description = "The configuration format to use.";
    };

    location = mkOption {
      type = types.oneOf [
        types.str
        (types.submodule {
          options = {
            lat = mkOption {
              type = types.float;
              description = "Latitude";
            };

            lon = mkOption {
              type = types.float;
              description = "Longitude";
            };
          };
        })
      ];

      default = {
        lat = 40.7128;
        lon = -74.0060;
      };

      description = ''
        Specifies the location for weather data.
        This can be either a city name (as a string) or an attribute set
        with `lat` and `lon` coordinates.
        Using a city name is only supported by the OpenWeatherMap provider
        and is not supported in `hpp` configuration mode.
      '';

      example = literalExpression ''
        "New York" or { lat = 40.7128; lon = -74.0060; }
      '';
    };

    weatherProvider = mkOption {
      type = types.enum ["OpenMeteo" "MetNo" "OpenWeatherMap"];
      default = "OpenMeteo";
      description = "The weather provider to use.";
    };

    weatherApiKey = mkOption {
      type = types.nullOr types.str;
      default = null;
      description = "API key, only required for OpenWeatherMap.";
    };

    usePugixml = mkOption {
      type = types.bool;
      default = false;
      description = "Use pugixml to parse XBPS package metadata. Required for package count functionality on Void Linux.";
    };

    enableNowPlaying = mkOption {
      type = types.bool;
      default = true;
      description = "Enable nowplaying functionality.";
    };

    enableWeather = mkOption {
      type = types.bool;
      default = true;
      description = "Enable fetching weather data.";
    };

    enablePackageCount = mkOption {
      type = types.bool;
      default = true;
      description = "Enable getting package count.";
    };

    username = mkOption {
      type = types.str;
      default = config.home.username // "User";
      description = "Username to display in the application.";
    };

    showTownName = mkOption {
      type = types.bool;
      default = true;
      description = "Show town name in weather display.";
    };

    weatherUnit = mkOption {
      type = types.enum ["metric" "imperial"];
      default = "metric";
      description = "Unit for temperature display.";
    };

    packageManagers = mkOption {
      type = types.listOf (types.enum (
        ["cargo" "nix"]
        ++ lib.optionals pkgs.stdenv.isLinux ["apk" "dpkg" "moss" "pacman" "rpm" "xbps"]
        ++ lib.optionals pkgs.stdenv.isDarwin ["homebrew" "macports"]
      ));
      default = [];
      description = "List of package managers to check for package counts.";
    };
  };

  config = mkIf cfg.enable {
    home.packages = [draconisPkg];

    xdg.configFile."draconis++/config.toml" = mkIf (cfg.configFormat == "toml") {
      source = tomlFormat.generate "config.toml" {
        location =
          if lib.isAttrs cfg.location
          then {inherit (cfg.location) lat lon;}
          else {name = cfg.location;};
        weather = {
          inherit (cfg) weatherProvider weatherApiKey;
        };
      };
    };

    assertions = [
      {
        assertion = !(lib.isString cfg.location && cfg.weatherProvider != "OpenWeatherMap");
        message = "A town/city name for the location can only be used with the OpenWeatherMap provider.";
      }
      {
        assertion = !(cfg.weatherApiKey != null && cfg.weatherProvider != "OpenWeatherMap");
        message = "An API key should not be provided when using the OpenMeteo or MetNo providers.";
      }
      {
        assertion = !(cfg.usePugixml && !cfg.enablePackageCount);
        message = "usePugixml should only be enabled when enablePackageCount is also enabled.";
      }
    ];
  };
}
