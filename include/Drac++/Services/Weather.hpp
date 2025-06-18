#pragma once

#if DRAC_ENABLE_WEATHER

  #include <glaze/core/common.hpp> // object
  #include <glaze/core/meta.hpp>   // Object
  #include <matchit.hpp>
  #include <variant>

  #include "DracUtils/Error.hpp"
  #include "DracUtils/Types.hpp"

namespace draconis::services::weather {
  /**
   * @brief Specifies the weather service provider.
   * @see config::DRAC_WEATHER_PROVIDER in `config(.example).hpp`.
   */
  enum class Provider : utils::types::u8 {
    OPENWEATHERMAP, ///< OpenWeatherMap API. Requires an API key. @see config::DRAC_API_KEY
    OPENMETEO,      ///< OpenMeteo API. Does not require an API key.
    METNO,          ///< Met.no API. Does not require an API key.
  };

  /**
   * @brief Specifies the unit system for weather information.
   * @see config::DRAC_WEATHER_UNIT in `config(.example).hpp`.
   */
  enum class Unit : utils::types::u8 {
    METRIC,   ///< Metric units (Celsius, kph, etc.).
    IMPERIAL, ///< Imperial units (Fahrenheit, mph, etc.).
  };

  /**
   * @struct Report
   * @brief Represents a weather report.
   *
   * Contains temperature, conditions, and timestamp.
   */
  struct Report {
    utils::types::f64                          temperature; ///< Degrees (C/F)
    utils::types::Option<utils::types::String> name;        ///< Optional town/city name (may be missing for some providers)
    utils::types::String                       description; ///< Weather description (e.g., "clear sky", "rain")
  };

  struct Coords {
    utils::types::f64 lat;
    utils::types::f64 lon;
  };

  using Location = std::variant<utils::types::String, Coords>;

  class IWeatherService {
   public:
    IWeatherService(const IWeatherService&) = delete;
    IWeatherService(IWeatherService&&)      = delete;

    fn operator=(const IWeatherService&)->IWeatherService& = delete;
    fn operator=(IWeatherService&&)->IWeatherService&      = delete;

    virtual ~IWeatherService() = default;

    [[nodiscard]] virtual fn getWeatherInfo() const -> utils::types::Result<Report> = 0;

   protected:
    IWeatherService() = default;
  };

  fn CreateWeatherService(Provider provider, const Location& location, Unit units, const utils::types::Option<utils::types::String>& apiKey = utils::types::None) -> utils::types::UniquePointer<IWeatherService>;
} // namespace draconis::services::weather

template <>
struct glz::meta<draconis::services::weather::Report> {
  using T = draconis::services::weather::Report;

  // clang-format off
  static constexpr detail::Object value = object(
    "temperature", &T::temperature,
    "name",        &T::name,
    "description", &T::description
  );
  // clang-format on
}; // namespace glz

template <>
struct std::formatter<draconis::services::weather::Unit> {
  static constexpr auto parse(std::format_parse_context& ctx) {
    return ctx.begin();
  }

  static fn format(draconis::services::weather::Unit unit, std::format_context& ctx) {
    using matchit::match, matchit::is, matchit::_;

    return std::format_to(ctx.out(), "{}", match(unit)(is | draconis::services::weather::Unit::METRIC = "metric", is | draconis::services::weather::Unit::IMPERIAL = "imperial"));
  }
};

#endif // DRAC_ENABLE_WEATHER
