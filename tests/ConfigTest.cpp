#include "Config/Config.hpp"

#include <toml++/toml.h>

#include "gtest/gtest.h"

class ConfigTest : public testing::Test {};

TEST_F(ConfigTest, GeneralFromToml_WithName) {
  auto tbl = toml::parse(R"(
        name = "Test User"
    )");
  ASSERT_TRUE(tbl.is_table());
  General generalConfig = General::fromToml(*tbl.as_table());
  EXPECT_EQ(generalConfig.name, "Test User");
}

TEST_F(ConfigTest, GeneralFromToml_DefaultName) {
  auto tbl = toml::parse(R"(
        # No name specified
    )");
  ASSERT_TRUE(tbl.is_table());
  General generalConfig = General::fromToml(*tbl.as_table());
  // We can't easily predict the default system name in a portable test,
  // but we can check it's not empty.
  EXPECT_FALSE(generalConfig.name.empty());
}

TEST_F(ConfigTest, NowPlayingFromToml_Enabled) {
  auto tbl = toml::parse(R"(
        enabled = true
    )");
  ASSERT_TRUE(tbl.is_table());
  NowPlaying npConfig = NowPlaying::fromToml(*tbl.as_table());
  EXPECT_TRUE(npConfig.enabled);
}

TEST_F(ConfigTest, NowPlayingFromToml_Disabled) {
  auto tbl = toml::parse(R"(
        enabled = false
    )");
  ASSERT_TRUE(tbl.is_table());
  NowPlaying npConfig = NowPlaying::fromToml(*tbl.as_table());
  EXPECT_FALSE(npConfig.enabled);
}

TEST_F(ConfigTest, NowPlayingFromToml_Default) {
  auto tbl = toml::parse(R"(
        # No enabled field
    )");
  ASSERT_TRUE(tbl.is_table());
  NowPlaying npConfig = NowPlaying::fromToml(*tbl.as_table());
  EXPECT_FALSE(npConfig.enabled); // Default is false
}

TEST_F(ConfigTest, WeatherFromToml_BasicEnabled) {
  auto tbl = toml::parse(R"(
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
  EXPECT_EQ(weatherConfig.units, "metric");
  EXPECT_TRUE(weatherConfig.showTownName);
  ASSERT_NE(weatherConfig.service, nullptr);
}

TEST_F(ConfigTest, WeatherFromToml_DisabledIfNoApiKey) {
  auto tbl = toml::parse(R"(
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
  auto tbl = toml::parse(R"(
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
  auto tbl = toml::parse(R"(
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
  // Check if it's an OpenMeteoService (requires RTTI or a type field if you want to be specific)
  // For now, just checking service is not null is a good start.
}

TEST_F(ConfigTest, WeatherFromToml_LocationCoords_MetNo) {
  auto tbl = toml::parse(R"(
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
  auto tbl = toml::parse(R"(
        enabled = true
        api_key = "test_key"
        location = 123 # Invalid type for location
    )");
  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled); // Should be disabled due to invalid location
}

TEST_F(ConfigTest, WeatherFromToml_MissingLocation) {
  auto tbl = toml::parse(R"(
        enabled = true
        api_key = "test_key"
        # location is missing
    )");
  ASSERT_TRUE(tbl.is_table());
  Weather weatherConfig = Weather::fromToml(*tbl.as_table());
  EXPECT_FALSE(weatherConfig.enabled); // Should be disabled due to missing location
}

TEST_F(ConfigTest, WeatherFromToml_OpenMeteoRequiresCoords) {
  auto tbl = toml::parse(R"(
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
  auto tbl = toml::parse(R"(
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
  auto tbl = toml::parse(R"(
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

// Test the main Config constructor
TEST_F(ConfigTest, MainConfigConstructor) {
  auto tomlTable = toml::parse(R"(
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
  EXPECT_TRUE(mainConfig.nowPlaying.enabled);
  EXPECT_TRUE(mainConfig.weather.enabled);
  EXPECT_EQ(mainConfig.weather.apiKey, "main_weather_key");
  ASSERT_TRUE(std::holds_alternative<String>(mainConfig.weather.location));
  EXPECT_EQ(std::get<String>(mainConfig.weather.location), "Main Test City");
  ASSERT_NE(mainConfig.weather.service, nullptr);
}

TEST_F(ConfigTest, MainConfigConstructor_EmptySections) {
  auto tomlTable = toml::parse(R"(
        # Empty config
    )");
  ASSERT_TRUE(tomlTable.is_table());

  Config mainConfig(*tomlTable.as_table());

  // Expect default values
  EXPECT_FALSE(mainConfig.general.name.empty()); // Default system name
  EXPECT_FALSE(mainConfig.nowPlaying.enabled);
  EXPECT_FALSE(mainConfig.weather.enabled);
  EXPECT_TRUE(mainConfig.weather.apiKey.empty());
  // Default location for weather is tricky as it's not set if weather is disabled.
  // The default constructor for Weather struct will be used.
  // Let's check that service is null as it would be if parsing failed or disabled.
  EXPECT_EQ(mainConfig.weather.service, nullptr);
}
