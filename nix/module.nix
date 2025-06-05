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
    if cfg.weatherApiKey == null || cfg.weatherApiKey == ""
    then "nullptr"
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
        constexpr Location DRAC_LOCATION = ${location};
        #endif

        #if DRAC_ENABLE_PACKAGECOUNT
        constexpr PackageManager DRAC_ENABLED_PACKAGE_MANAGERS = ${builtins.concatStringsSep " | " (map (pkg: "PackageManager::" + lib.toUpper pkg) cfg.packageManagers)};
        #endif
      }

      #endif
    '';

  buildMesonFlags = {
    precompiled_config = true;
    precompiled_config_path = configHpp;
    use_pugixml = cfg.usePugixml;
    enable_nowplaying = cfg.enableNowPlaying;
    enable_weather = cfg.enableWeather;
    enable_packagecount = cfg.enablePackagecount;
  };

  mesonFlagsList =
    lib.mapAttrsToList (
      name: value:
        if lib.isBool value
        then "-D${name}=${
          if value
          then "true"
          else "false"
        }"
        else "-D${name}=${toString value}"
    )
    buildMesonFlags;

  draconisWithOverrides = cfg.package.overrideAttrs (oldAttrs: {
    mesonFlags = (oldAttrs.mesonFlags or []) ++ mesonFlagsList;
  });

  draconisPkg = draconisWithOverrides;
in {
  options.programs.draconisplusplus = {
    enable = mkEnableOption "draconis++";

    package = mkOption {
      type = types.package;
      default = defaultPackage;
      description = "The base draconis++ package. Used directly if 'configFormat' is 'toml', or as a base for overriding if 'configFormat' is 'hpp'.";
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
      type = types.str;
      default = "";
      description = "API key for your chosen weather service.";
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

    enablePackagecount = mkOption {
      type = types.bool;
      default = true;
      description = "Enable getting package count.";
    };

    username = mkOption {
      type = types.str;
      default = "";
      description = "Username to display in the application.";
    };

    showTownName = mkOption {
      type = types.bool;
      default = true;
      description = "Show town name in weather display.";
    };

    weatherUnit = mkOption {
      type = types.enum ["celsius" "fahrenheit"];
      default = "celsius";
      description = "Unit for temperature display.";
    };

    packageManagers = mkOption {
      type = types.listOf (types.enum ["apt" "pacman" "cargo" "dnf" "xbps" "portage" "brew" "nix"]);
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
        assertion = !(cfg.weatherApiKey != "" && cfg.weatherProvider != "OpenWeatherMap");
        message = "An API key should not be provided when using the OpenMeteo or MetNo providers.";
      }
      {
        assertion = !(cfg.usePugixml && !cfg.enablePackagecount);
        message = "usePugixml should only be enabled when enablePackagecount is also enabled.";
      }
      {
        assertion = !(cfg.weatherApiKey != "" && !cfg.enableWeather);
        message = "weatherApiKey should only be provided when enableWeather is enabled.";
      }
    ];
  };
}
