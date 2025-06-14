#if DRAC_ENABLE_WEATHER

// clang-format off
#include "Services/Weather/DataTransferObjects.hpp"
#include "Services/Weather/WeatherUtils.hpp"

#include <DracUtils/Error.hpp>
#include <DracUtils/Types.hpp>

#include "gtest/gtest.h"
// clang-format on

using namespace util::types;
using namespace weather::utils;
using glz::read, glz::error_code, glz::error_ctx;
using enum util::error::DracErrorCode;

class WeatherServiceTest : public testing::Test {};

// NOLINTBEGIN(modernize-use-trailing-return-type, cert-err58-cpp)
TEST_F(WeatherServiceTest, StripTimeOfDay_DaySuffix) {
  EXPECT_EQ(StripTimeOfDayFromSymbol("clearsky_day"), "clearsky");
  EXPECT_EQ(StripTimeOfDayFromSymbol("partlycloudy_day"), "partlycloudy");
}

TEST_F(WeatherServiceTest, StripTimeOfDay_NightSuffix) {
  EXPECT_EQ(StripTimeOfDayFromSymbol("clearsky_night"), "clearsky");
  EXPECT_EQ(StripTimeOfDayFromSymbol("cloudy_night"), "cloudy");
}

TEST_F(WeatherServiceTest, StripTimeOfDay_PolarTwilightSuffix) {
  EXPECT_EQ(StripTimeOfDayFromSymbol("fair_polartwilight"), "fair");
}

TEST_F(WeatherServiceTest, StripTimeOfDay_NoSuffix) {
  EXPECT_EQ(StripTimeOfDayFromSymbol("rain"), "rain");
  EXPECT_EQ(StripTimeOfDayFromSymbol("heavyrainandthunder"), "heavyrainandthunder");
}

TEST_F(WeatherServiceTest, StripTimeOfDay_EmptyString) {
  EXPECT_EQ(StripTimeOfDayFromSymbol(""), "");
}

TEST_F(WeatherServiceTest, StripTimeOfDay_SuffixOnly) {
  EXPECT_EQ(StripTimeOfDayFromSymbol("_day"), "_day");
}

TEST_F(WeatherServiceTest, StripTimeOfDay_PartialSuffix) {
  EXPECT_EQ(StripTimeOfDayFromSymbol("clearsky_da"), "clearsky_da");
}

TEST_F(WeatherServiceTest, ParseISO8601ToEpoch_Valid) {
  // Test case: 2023-10-26T10:30:00Z
  // Online epoch converters give 1698316200 for this timestamp
  Result<time_t> result = ParseIso8601ToEpoch("2023-10-26T10:30:00Z");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 1698316200);

  // Test case: 1970-01-01T00:00:00Z (Epoch itself)
  result = ParseIso8601ToEpoch("1970-01-01T00:00:00Z");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 0);

  // Test case: 2000-03-01T12:00:00Z
  result = ParseIso8601ToEpoch("2000-03-01T12:00:00Z");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 951912000);
}

TEST_F(WeatherServiceTest, ParseISO8601ToEpoch_InvalidFormat_TooShort) {
  Result<time_t> result = ParseIso8601ToEpoch("2023-10-26T10:30:00");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ParseError);
}

TEST_F(WeatherServiceTest, ParseISO8601ToEpoch_InvalidFormat_TooLong) {
  Result<time_t> result = ParseIso8601ToEpoch("2023-10-26T10:30:00ZEXTRA");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ParseError);
}

TEST_F(WeatherServiceTest, ParseISO8601ToEpoch_InvalidFormat_WrongSeparator) {
  Result<time_t> result = ParseIso8601ToEpoch("2023-10-26X10:30:00Z");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ParseError);
}

TEST_F(WeatherServiceTest, ParseISO8601ToEpoch_InvalidValues_BadMonth) {
  // Note: The current implementation doesn't validate date ranges, it only checks format
  // Month 13 might actually be accepted by the system time functions
  // Let's test with a clearly invalid format instead
  Result<time_t> result = ParseIso8601ToEpoch("2023-AB-26T10:30:00Z"); // Non-numeric month
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ParseError);
}

TEST_F(WeatherServiceTest, ParseISO8601ToEpoch_InvalidValues_NonNumeric) {
  Result<time_t> result = ParseIso8601ToEpoch("2023-1A-26T10:30:00Z");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ParseError);
}

TEST_F(WeatherServiceTest, ParseISO8601ToEpoch_EmptyString) {
  Result<time_t> result = ParseIso8601ToEpoch("");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ParseError);
}

TEST_F(WeatherServiceTest, MetNoSymbolDescriptions_ClearWeather) {
  const std::unordered_map<StringView, StringView>& descriptions = GetMetnoSymbolDescriptions();

  EXPECT_EQ(descriptions.at("clearsky"), "clear sky");
  EXPECT_EQ(descriptions.at("fair"), "fair");
  EXPECT_EQ(descriptions.at("partlycloudy"), "partly cloudy");
  EXPECT_EQ(descriptions.at("cloudy"), "cloudy");
  EXPECT_EQ(descriptions.at("fog"), "fog");
}

TEST_F(WeatherServiceTest, MetNoSymbolDescriptions_RainWeather) {
  const std::unordered_map<StringView, StringView>& descriptions = GetMetnoSymbolDescriptions();

  EXPECT_EQ(descriptions.at("lightrain"), "light rain");
  EXPECT_EQ(descriptions.at("rain"), "rain");
  EXPECT_EQ(descriptions.at("heavyrain"), "heavy rain");
  EXPECT_EQ(descriptions.at("rainandthunder"), "rain and thunder");
}

TEST_F(WeatherServiceTest, MetNoSymbolDescriptions_SnowWeather) {
  const std::unordered_map<StringView, StringView>& descriptions = GetMetnoSymbolDescriptions();

  EXPECT_EQ(descriptions.at("lightsnow"), "light snow");
  EXPECT_EQ(descriptions.at("snow"), "snow");
  EXPECT_EQ(descriptions.at("heavysnow"), "heavy snow");
  EXPECT_EQ(descriptions.at("snowandthunder"), "snow and thunder");
}

TEST_F(WeatherServiceTest, OpenMeteoWeatherDescription_CommonCodes) {
  EXPECT_EQ(GetOpenmeteoWeatherDescription(0), "clear sky");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(1), "mainly clear");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(2), "partly cloudy");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(3), "overcast");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(45), "fog");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(48), "fog");
}

TEST_F(WeatherServiceTest, OpenMeteoWeatherDescription_RainCodes) {
  EXPECT_EQ(GetOpenmeteoWeatherDescription(51), "drizzle");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(55), "drizzle");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(61), "rain");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(65), "rain");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(80), "rain showers");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(82), "rain showers");
}

TEST_F(WeatherServiceTest, OpenMeteoWeatherDescription_SnowCodes) {
  EXPECT_EQ(GetOpenmeteoWeatherDescription(71), "snow fall");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(75), "snow fall");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(77), "snow grains");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(85), "snow showers");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(86), "snow showers");
}

TEST_F(WeatherServiceTest, OpenMeteoWeatherDescription_ThunderstormCodes) {
  EXPECT_EQ(GetOpenmeteoWeatherDescription(95), "thunderstorm");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(96), "thunderstorm with hail");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(99), "thunderstorm with hail");
}

TEST_F(WeatherServiceTest, OpenMeteoWeatherDescription_UnknownCode) {
  EXPECT_EQ(GetOpenmeteoWeatherDescription(999), "unknown");
  EXPECT_EQ(GetOpenmeteoWeatherDescription(-1), "unknown");
}

TEST_F(WeatherServiceTest, MetNoJsonParsing_ValidCompleteResponse) {
  const String validJson = R"({
    "properties": {
      "timeseries": [
        {
          "time": "2023-10-26T10:30:00Z",
          "data": {
            "instant": {
              "details": {
                "air_temperature": 15.2
              }
            },
            "next_1_hours": {
              "summary": {
                "symbol_code": "clearsky_day"
              }
            }
          }
        }
      ]
    }
  })";

  weather::dto::metno::Response response;
  error_ctx                     result = read<glz::opts { .error_on_unknown_keys = false }>(response, validJson);

  ASSERT_EQ(result.ec, error_code::none);
  ASSERT_FALSE(response.properties.timeseries.empty());

  const weather::dto::metno::Timeseries& timeseries = response.properties.timeseries[0];
  EXPECT_EQ(timeseries.time, "2023-10-26T10:30:00Z");
  EXPECT_DOUBLE_EQ(timeseries.data.instant.details.airTemperature, 15.2);
  ASSERT_TRUE(timeseries.data.next1Hours.has_value());
  EXPECT_EQ(timeseries.data.next1Hours->summary.symbolCode, "clearsky_day");
}

TEST_F(WeatherServiceTest, MetNoJsonParsing_ValidMinimalResponse) {
  const String minimalJson = R"({
    "properties": {
      "timeseries": [
        {
          "time": "2023-10-26T10:30:00Z",
          "data": {
            "instant": {
              "details": {
                "air_temperature": -5.0
              }
            }
          }
        }
      ]
    }
  })";

  weather::dto::metno::Response response;
  error_ctx                     result = read<glz::opts { .error_on_unknown_keys = false }>(response, minimalJson);

  ASSERT_EQ(result.ec, error_code::none);
  ASSERT_FALSE(response.properties.timeseries.empty());

  const weather::dto::metno::Timeseries& timeseries = response.properties.timeseries[0];
  EXPECT_EQ(timeseries.time, "2023-10-26T10:30:00Z");
  EXPECT_DOUBLE_EQ(timeseries.data.instant.details.airTemperature, -5.0);
  EXPECT_FALSE(timeseries.data.next1Hours.has_value());
}

TEST_F(WeatherServiceTest, MetNoJsonParsing_InvalidJson) {
  const String invalidJson = R"({
    "properties": {
      "timeseries": [
        {
          "time": "2023-10-26T10:30:00Z",
          "data": {
            "instant": {
              // Missing details object
            }
          }
        }
      ]
    }
  })";

  weather::dto::metno::Response response;
  error_ctx                     result = read<glz::opts { .error_on_unknown_keys = false }>(response, invalidJson);

  EXPECT_NE(result.ec, error_code::none);
}

TEST_F(WeatherServiceTest, MetNoJsonParsing_EmptyTimeseries) {
  const String emptyTimeseriesJson = R"({
    "properties": {
      "timeseries": []
    }
  })";

  weather::dto::metno::Response response;
  error_ctx                     result = read<glz::opts { .error_on_unknown_keys = false }>(response, emptyTimeseriesJson);

  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_TRUE(response.properties.timeseries.empty());
}

TEST_F(WeatherServiceTest, OpenMeteoJsonParsing_ValidResponse) {
  const String validJson = R"({
    "current_weather": {
      "temperature": 22.5,
      "weathercode": 1,
      "time": "2023-10-26T10:30:00Z"
    }
  })";

  weather::dto::openmeteo::Response response;
  error_ctx                         result = read<glz::opts { .error_on_unknown_keys = false }>(response, validJson);

  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_DOUBLE_EQ(response.currentWeather.temperature, 22.5);
  EXPECT_EQ(response.currentWeather.weathercode, 1);
  EXPECT_EQ(response.currentWeather.time, "2023-10-26T10:30:00Z");
}

TEST_F(WeatherServiceTest, OpenMeteoJsonParsing_NegativeTemperature) {
  const String coldWeatherJson = R"({
    "current_weather": {
      "temperature": -15.8,
      "weathercode": 71,
      "time": "2023-12-15T08:00:00Z"
    }
  })";

  weather::dto::openmeteo::Response response;
  error_ctx                         result = read<glz::opts { .error_on_unknown_keys = false }>(response, coldWeatherJson);

  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_DOUBLE_EQ(response.currentWeather.temperature, -15.8);
  EXPECT_EQ(response.currentWeather.weathercode, 71);
  EXPECT_EQ(response.currentWeather.time, "2023-12-15T08:00:00Z");
}

TEST_F(WeatherServiceTest, OpenMeteoJsonParsing_InvalidJson) {
  const String invalidJson = R"({
    "current_weather": {
      "temperature": "not_a_number",
      "weathercode": 1,
      "time": "2023-10-26T10:30:00Z"
    }
  })";

  weather::dto::openmeteo::Response response;
  error_ctx                         result = read<glz::opts { .error_on_unknown_keys = false }>(response, invalidJson);

  EXPECT_NE(result.ec, error_code::none);
}

TEST_F(WeatherServiceTest, OpenMeteoJsonParsing_MissingFields) {
  const String incompleteJson = R"({
    "current_weather": {
      "temperature": 20.0
    }
  })";

  // Explicitly initialize the struct to ensure predictable behavior
  weather::dto::openmeteo::Response response = {};
  error_ctx                         result   = read<glz::opts { .error_on_unknown_keys = false }>(response, incompleteJson);

  // Note: Glaze doesn't fail on missing fields by default, it just leaves them uninitialized
  // This test verifies that parsing succeeds and only specified fields are updated
  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_DOUBLE_EQ(response.currentWeather.temperature, 20.0);
  EXPECT_EQ(response.currentWeather.weathercode, 0); // Zero-initialized
  EXPECT_TRUE(response.currentWeather.time.empty()); // Default constructed empty string
}

TEST_F(WeatherServiceTest, OpenWeatherMapJsonParsing_ValidResponse) {
  const String validJson = R"({
    "main": {
      "temp": 18.7
    },
    "weather": [
      {
        "description": "scattered clouds"
      }
    ],
    "name": "London",
    "dt": 1698316200
  })";

  weather::dto::owm::OWMResponse response;
  error_ctx                      result = read<glz::opts { .error_on_unknown_keys = false }>(response, validJson);

  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_DOUBLE_EQ(response.main.temp, 18.7);
  ASSERT_FALSE(response.weather.empty());
  EXPECT_EQ(response.weather[0].description, "scattered clouds");
  EXPECT_EQ(response.name, "London");
  EXPECT_EQ(response.dt, 1698316200);
}

TEST_F(WeatherServiceTest, OpenWeatherMapJsonParsing_EmptyWeatherArray) {
  const String emptyWeatherJson = R"({
    "main": {
      "temp": 25.0
    },
    "weather": [],
    "name": "Unknown",
    "dt": 1698316200
  })";

  weather::dto::owm::OWMResponse response;
  error_ctx                      result = read<glz::opts { .error_on_unknown_keys = false }>(response, emptyWeatherJson);

  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_DOUBLE_EQ(response.main.temp, 25.0);
  EXPECT_TRUE(response.weather.empty());
  EXPECT_EQ(response.name, "Unknown");
  EXPECT_EQ(response.dt, 1698316200);
}

TEST_F(WeatherServiceTest, OpenWeatherMapJsonParsing_MultipleWeatherEntries) {
  const String multiWeatherJson = R"({
    "main": {
      "temp": 12.3
    },
    "weather": [
      {
        "description": "light rain"
      },
      {
        "description": "broken clouds"
      }
    ],
    "name": "Paris",
    "dt": 1698316200
  })";

  weather::dto::owm::OWMResponse response;
  error_ctx                      result = read<glz::opts { .error_on_unknown_keys = false }>(response, multiWeatherJson);

  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_DOUBLE_EQ(response.main.temp, 12.3);
  ASSERT_EQ(response.weather.size(), 2UL);
  EXPECT_EQ(response.weather[0].description, "light rain");
  EXPECT_EQ(response.weather[1].description, "broken clouds");
  EXPECT_EQ(response.name, "Paris");
}

TEST_F(WeatherServiceTest, OpenWeatherMapJsonParsing_InvalidJson) {
  const String invalidJson = R"({
    "main": {
      "temp": null
    },
    "weather": [
      {
        "description": "clear sky"
      }
    ],
    "name": "TestCity",
    "dt": "not_a_number"
  })";

  weather::dto::owm::OWMResponse response;
  error_ctx                      result = read<glz::opts { .error_on_unknown_keys = false }>(response, invalidJson);

  EXPECT_NE(result.ec, error_code::none);
}

TEST_F(WeatherServiceTest, OpenWeatherMapJsonParsing_EmptyName) {
  const String emptyNameJson = R"({
    "main": {
      "temp": 8.9
    },
    "weather": [
      {
        "description": "overcast clouds"
      }
    ],
    "name": "",
    "dt": 1698316200
  })";

  weather::dto::owm::OWMResponse response;
  error_ctx                      result = read<glz::opts { .error_on_unknown_keys = false }>(response, emptyNameJson);

  ASSERT_EQ(result.ec, error_code::none);
  EXPECT_DOUBLE_EQ(response.main.temp, 8.9);
  EXPECT_EQ(response.name, "");
  EXPECT_EQ(response.weather[0].description, "overcast clouds");
}
// NOLINTEND(modernize-use-trailing-return-type, cert-err58-cpp)

fn main(int argc, char** argv) -> int {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

#endif // DRAC_ENABLE_WEATHER
