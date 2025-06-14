#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include <format>
#include <glaze/core/common.hpp> // object
#include <glaze/core/meta.hpp>   // Object
#include <matchit.hpp>

#include "DracUtils/Types.hpp"
#include "DracUtils/Error.hpp"
// clang-format on

struct Config;

namespace weather {
  /**
   * @brief Specifies the weather service provider.
   * @see config::DRAC_WEATHER_PROVIDER in `config.example.hpp` or `config.hpp`.
   */
  enum class Provider : util::types::u8 {
    OPENWEATHERMAP, ///< OpenWeatherMap API. Requires an API key. @see config::DRAC_API_KEY
    OPENMETEO,      ///< OpenMeteo API. Does not require an API key.
    METNO,          ///< Met.no API. Does not require an API key.
  };

  /**
   * @brief Specifies the unit system for weather information.
   * @see config::DRAC_WEATHER_UNIT in `config.example.hpp` or `config.hpp`.
   */
  enum class Unit : util::types::u8 {
    METRIC,   ///< Metric units (Celsius, kph, etc.).
    IMPERIAL, ///< Imperial units (Fahrenheit, mph, etc.).
  };

  /**
   * @struct WeatherReport
   * @brief Represents a weather report.
   *
   * Contains temperature, conditions, and timestamp.
   */
  struct WeatherReport {
    util::types::f64                         temperature; ///< Degrees (C/F)
    util::types::Option<util::types::String> name;        ///< Optional town/city name (may be missing for some providers)
    util::types::String                      description; ///< Weather description (e.g., "clear sky", "rain")
  };

  /**
   * @brief Fetches the weather information.
   * @param config The configuration object containing settings for the weather.
   * @return Result containing the weather information.
   */
  fn GetWeatherInfo(const Config& config) -> util::types::Result<WeatherReport>;

  struct Coords {
    util::types::f64 lat;
    util::types::f64 lon;
  };
} // namespace weather

namespace glz {
  template <>
  struct meta<weather::WeatherReport> {
    // clang-format off
    static constexpr detail::Object value = object(
      "temperature", &weather::WeatherReport::temperature,
      "name",        &weather::WeatherReport::name,
      "description", &weather::WeatherReport::description
    );
    // clang-format on
  };
} // namespace glz

template <>
struct std::formatter<weather::Unit> {
  static constexpr auto parse(std::format_parse_context& ctx) {
    return ctx.begin();
  }

  static fn format(weather::Unit unit, std::format_context& ctx) {
    using matchit::match, matchit::is, matchit::_;

    return std::format_to(ctx.out(), "{}", match(unit)(is | weather::Unit::METRIC = "metric", is | weather::Unit::IMPERIAL = "imperial"));
  }
};

#endif // DRAC_ENABLE_WEATHER