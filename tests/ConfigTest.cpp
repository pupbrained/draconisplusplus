#include "gtest/gtest.h"

class ConfigTest : public testing::Test {};

#ifdef PRECOMPILED_CONFIG

  #include <type_traits> // For std::is_same_v

  #include "config.hpp" // Include the precompiled configuration header

TEST_F(ConfigTest, PrecompiledConfigTypes) {
  // General
  EXPECT_TRUE((std::is_same_v<decltype(config::DRAC_USERNAME), const char* const>));

  #if DRAC_ENABLE_WEATHER
  // Weather
  EXPECT_TRUE((std::is_same_v<decltype(config::DRAC_WEATHER_PROVIDER), const config::WeatherProvider>));
  EXPECT_TRUE((std::is_same_v<decltype(config::DRAC_WEATHER_UNIT), const config::WeatherUnit>));
  EXPECT_TRUE((std::is_same_v<decltype(config::DRAC_SHOW_TOWN_NAME), const bool>));
  EXPECT_TRUE((std::is_same_v<decltype(config::DRAC_API_KEY), const char* const>));
  EXPECT_TRUE((std::is_same_v<decltype(config::DRAC_LOCATION), const Location>));
  #endif // DRAC_ENABLE_WEATHER

  #if DRAC_ENABLE_PACKAGECOUNT
  // Package Count
  EXPECT_TRUE((std::is_same_v<decltype(config::DRAC_ENABLED_PACKAGE_MANAGERS), const config::PackageManager>));
  #endif // DRAC_ENABLE_PACKAGECOUNT
}

#else
  #include <toml++/toml.h> // toml::{parse_result, parse}
  #include <variant>

  #include "Config/Config.hpp"

  #include "Services/Weather.hpp" // For weather::Coords

TEST_F(ConfigTest, GeneralFromToml_WithName) {
  toml::parse_result tbl = toml::parse(R"(
    name = "Test User"
  )");

  ASSERT_TRUE(tbl.is_table());
  General generalConfig = General::fromToml(*tbl.as_table());
  EXPECT_EQ(generalConfig.name, "Test User");
}

TEST_F(ConfigTest, GeneralFromToml_DefaultName) {
  toml::parse_result tbl = toml::parse(R"(
    # No name specified
  )");

  ASSERT_TRUE(tbl.is_table());
  General generalConfig = General::fromToml(*tbl.as_table());
  EXPECT_FALSE(generalConfig.name.empty());
}

  #if DRAC_ENABLE_NOWPLAYING
TEST_F(ConfigTest, NowPlayingFromToml_Enabled) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
  )");

  ASSERT_TRUE(tbl.is_table());
  NowPlaying npConfig = NowPlaying::fromToml(*tbl.as_table());
  EXPECT_TRUE(npConfig.enabled);
}

TEST_F(ConfigTest, NowPlayingFromToml_Disabled) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = false
  )");

  ASSERT_TRUE(tbl.is_table());
  NowPlaying npConfig = NowPlaying::fromToml(*tbl.as_table());
  EXPECT_FALSE(npConfig.enabled);
}

TEST_F(ConfigTest, NowPlayingFromToml_Default) {
  toml::parse_result tbl = toml::parse(R"(
    # No enabled field
  )");

  ASSERT_TRUE(tbl.is_table());
  NowPlaying npConfig = NowPlaying::fromToml(*tbl.as_table());
  EXPECT_FALSE(npConfig.enabled); // Default should be false
}
  #endif // DRAC_ENABLE_NOWPLAYING

  #if DRAC_ENABLE_WEATHER
TEST_F(ConfigTest, WeatherFromToml_BasicEnabled) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "test_key"
    location = "Test City"
    units = "metric"
    show_town_name = true
    provider = "openweathermap"
  )");
  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_TRUE(weatherConfig.enabled);
  EXPECT_EQ(weatherConfig.apiKey, "test_key");
  ASSERT_TRUE(std::holds_alternative<String>(weatherConfig.location));
  EXPECT_EQ(std::get<String>(weatherConfig.location), "Test City");
  EXPECT_EQ(weatherConfig.units, config::WeatherUnit::METRIC);
  EXPECT_TRUE(weatherConfig.showTownName);
  ASSERT_NE(weatherConfig.service, nullptr);
}

TEST_F(ConfigTest, WeatherFromToml_DisabledIfNoApiKey) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    # api_key missing
    location = "Test City"
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled);
  EXPECT_EQ(weatherConfig.service, nullptr);
}

TEST_F(ConfigTest, WeatherFromToml_DisabledIfEnabledFalse) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = false
    api_key = "test_key"
      location = "Test City"
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled);
  EXPECT_EQ(weatherConfig.service, nullptr);
}

TEST_F(ConfigTest, WeatherFromToml_LocationCoords_OpenMeteo) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "dummy_key_not_used_by_openmeteo" # OpenMeteo doesn't use API key from here
    provider = "openmeteo"
    [location]
    lat = 12.34
    lon = 56.78
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_TRUE(weatherConfig.enabled);
  ASSERT_TRUE(std::holds_alternative<weather::Coords>(weatherConfig.location));
  weather::Coords coords = std::get<weather::Coords>(weatherConfig.location);
  EXPECT_DOUBLE_EQ(coords.lat, 12.34);
  EXPECT_DOUBLE_EQ(coords.lon, 56.78);
  ASSERT_NE(weatherConfig.service, nullptr);
}

TEST_F(ConfigTest, WeatherFromToml_LocationCoords_MetNo) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "dummy_key_not_used_by_metno" # MetNo doesn't use API key from here
    provider = "metno"
    [location]
    lat = 43.21
    lon = 87.65
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_TRUE(weatherConfig.enabled);
  ASSERT_TRUE(std::holds_alternative<weather::Coords>(weatherConfig.location));
  weather::Coords coords = std::get<weather::Coords>(weatherConfig.location);
  EXPECT_DOUBLE_EQ(coords.lat, 43.21);
  EXPECT_DOUBLE_EQ(coords.lon, 87.65);
  ASSERT_NE(weatherConfig.service, nullptr);
}

TEST_F(ConfigTest, WeatherFromToml_InvalidLocationType) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "test_key"
    location = 123 # Invalid type for location
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled);
}

TEST_F(ConfigTest, WeatherFromToml_MissingLocation) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "test_key"
    # location is missing
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled);
}

TEST_F(ConfigTest, WeatherFromToml_OpenMeteoRequiresCoords) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "dummy_key"
    provider = "openmeteo"
    location = "SomeCity" # OpenMeteo needs coordinates
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled);
}

TEST_F(ConfigTest, WeatherFromToml_MetNoRequiresCoords) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "dummy_key"
    provider = "metno"
    location = "SomeCity" # MetNo needs coordinates
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled);
}

TEST_F(ConfigTest, WeatherFromToml_UnknownProvider) {
  toml::parse_result tbl = toml::parse(R"(
    enabled = true
    api_key = "test_key"
    provider = "unknown_weather_service"
    location = "Test City"
  )");

  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled);
  EXPECT_EQ(weatherConfig.service, nullptr);
}
  #endif

TEST_F(ConfigTest, MainConfigConstructor) {
  toml::parse_result tomlTable = toml::parse(R"(
      [general]
      name = "Main Test User"

      [now_playing]
      enabled = true

    [weather]
    enabled = true
    api_key = "main_weather_key"
    location = "Main Test City"
    provider = "openweathermap"
  )");

  ASSERT_TRUE(tomlTable.is_table());

  Config mainConfig(*tomlTable.as_table());

  EXPECT_EQ(mainConfig.general.name, "Main Test User");
  #if DRAC_ENABLE_NOWPLAYING
  EXPECT_TRUE(mainConfig.nowPlaying.enabled);
  #endif
  #if DRAC_ENABLE_WEATHER
  EXPECT_TRUE(mainConfig.weather.enabled);
  EXPECT_EQ(mainConfig.weather.apiKey, "main_weather_key");
  ASSERT_TRUE(std::holds_alternative<String>(mainConfig.weather.location));
  EXPECT_EQ(std::get<String>(mainConfig.weather.location), "Main Test City");
  ASSERT_NE(mainConfig.weather.service, nullptr);
  #endif
}

TEST_F(ConfigTest, MainConfigConstructor_EmptySections) {
  toml::parse_result tomlTable = toml::parse(R"(
    # Empty config
  )");

  ASSERT_TRUE(tomlTable.is_table());

  Config mainConfig(*tomlTable.as_table());

  EXPECT_FALSE(mainConfig.general.name.empty());
  #if DRAC_ENABLE_NOWPLAYING
  EXPECT_FALSE(mainConfig.nowPlaying.enabled);
  #endif
  #if DRAC_ENABLE_WEATHER
  EXPECT_FALSE(mainConfig.weather.enabled);
  EXPECT_FALSE(mainConfig.weather.apiKey.has_value());
  EXPECT_EQ(mainConfig.weather.service, nullptr);
  #endif
}

#endif // PRECOMPILED_CONFIG