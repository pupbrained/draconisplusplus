#pragma once

#if DRAC_ENABLE_WEATHER

  #include <glaze/core/common.hpp> // object
  #include <glaze/core/meta.hpp>   // Object
  #include <matchit.hpp>
  #include <variant>

  #include "DracUtils/Error.hpp"
  #include "DracUtils/Types.hpp"

namespace weather {
  /**
   * @brief Specifies the weather service provider.
   * @see config::DRAC_WEATHER_PROVIDER in `config(.example).hpp`.
   */
  enum class Provider : util::types::u8 {
    OPENWEATHERMAP, ///< OpenWeatherMap API. Requires an API key. @see config::DRAC_API_KEY
    OPENMETEO,      ///< OpenMeteo API. Does not require an API key.
    METNO,          ///< Met.no API. Does not require an API key.
  };

  /**
   * @brief Specifies the unit system for weather information.
   * @see config::DRAC_WEATHER_UNIT in `config(.example).hpp`.
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

  struct Coords {
    util::types::f64 lat;
    util::types::f64 lon;
  };

  using Location = std::variant<util::types::String, Coords>;

  class IWeatherService {
   public:
    IWeatherService(const IWeatherService&) = delete;
    IWeatherService(IWeatherService&&)      = delete;

    fn operator=(const IWeatherService&)->IWeatherService& = delete;
    fn operator=(IWeatherService&&)->IWeatherService&      = delete;

    virtual ~IWeatherService() = default;

    [[nodiscard]] virtual fn getWeatherInfo() const -> util::types::Result<WeatherReport> = 0;

   protected:
    IWeatherService() = default;
  };

  fn CreateWeatherService(Provider provider, const Location& location, const util::types::String& apiKey, Unit units) -> util::types::UniquePointer<IWeatherService>;
  fn CreateWeatherService(Provider provider, const Coords& coords, Unit units) -> util::types::UniquePointer<IWeatherService>;
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
