{
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.draconis;

  apiKey =
    if cfg.weatherApiKey == null || cfg.weatherApiKey == ""
    then "nullptr"
    else cfg.weatherApiKey;

  configHpp = pkgs.writeText "config.hpp" ''
    #pragma once

    #ifdef PRECOMPILED_CONFIG

    #if DRAC_ENABLE_WEATHER || DRAC_ENABLE_PACKAGECOUNT
      #include "Config/Config.hpp"
      #include "Services/Weather.hpp"
      #include "Util/ConfigData.hpp"
    #endif

    namespace config {
      constexpr const char* DRAC_USERNAME = "${cfg.username}";

      #if DRAC_ENABLE_WEATHER
      constexpr WeatherProvider DRAC_WEATHER_PROVIDER = WeatherProvider::${lib.toUpper cfg.weatherProvider};
      constexpr WeatherUnit DRAC_WEATHER_UNIT = WeatherUnit::${lib.toUpper cfg.weatherUnit};
      constexpr bool DRAC_SHOW_TOWN_NAME = ${toString cfg.showTownName};
      constexpr const char* DRAC_API_KEY = ${apiKey};
      constexpr Location DRAC_LOCATION = weather::Coords { .lat = ${toString cfg.location.lat}, .lon = ${toString cfg.location.lon} };
      #endif

      #if DRAC_ENABLE_PACKAGECOUNT
      constexpr PackageManager DRAC_ENABLED_PACKAGE_MANAGERS = ${builtins.concatStringsSep " | " (map (pkg: "PackageManager::" + lib.toUpper pkg) cfg.packageManagers)};
      #endif
    }

    #endif
  '';

  draconisWithHpp = pkgs.stdenv.mkDerivation (finalAttrs: {
    pname = "draconis-plus-plus-hpp";
    version = "0.1.0";
    src = cfg.sourcePath;
    nativeBuildInputs = [pkgs.cmake];
    buildInputs = [
      pkgs.curl
      pkgs.sqlite
      pkgs.openssl
      pkgs.zlib
      pkgs.libxcb
      pkgs.wayland
      pkgs.dbus
    ];
    cmakeFlags =
      finalAttrs.cmakeFlags
      ++ [
        "-DDRACONIS_CONFIG_FILE=${configHpp}"
      ];
  });

  draconisPkg =
    if cfg.configFormat == "hpp"
    then draconisWithHpp
    else cfg.package;
in {
  options.programs.draconis = {
    enable = mkEnableOption "draconis++";

    package = mkOption {
      type = types.package;
      default = null;
      description = "The draconis++ package to use when configFormat is 'toml'.";
    };

    sourcePath = mkOption {
      type = types.path;
      default = null;
      description = "The path to the draconis++ source code. Required when `configFormat` is 'hpp'.";
    };

    configFormat = mkOption {
      type = types.enum ["toml" "hpp"];
      default = "toml";
      description = "The configuration format to use.";
    };

    # The new, unified location option
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
      type = types.str;
      default = "";
      description = "API key for your chosen weather service.";
    };
  };

  config = mkIf cfg.enable {
    home.packages = [draconisPkg];

    xdg.configFile."draconis/config.toml" = mkIf (cfg.configFormat == "toml") {
      text = lib.generators.toToml {} {
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
        assertion = !(cfg.weatherApiKey != "" && (cfg.weatherProvider == "OpenMeteo" || cfg.weatherProvider == "MetNo"));
        message = "An API key should not be provided when using the OpenMeteo or MetNo providers.";
      }
      {
        assertion = !(cfg.configFormat == "hpp" && cfg.sourcePath == null);
        message = "A sourcePath must be provided when using the hpp config format.";
      }
    ];
  };
}
