#pragma once

#if DRAC_ENABLE_WEATHER

  #include <glaze/core/common.hpp> // object
  #include <glaze/core/meta.hpp>   // Object
  #include <matchit.hpp>
  #include <variant>

  #include "../Utils/CacheManager.hpp"
  #include "../Utils/Types.hpp"

namespace draconis::services::weather {
  namespace {
    using utils::cache::CacheManager;

    using utils::types::f64;
    using utils::types::None;
    using utils::types::Option;
    using utils::types::Result;
    using utils::types::String;
    using utils::types::u8;
    using utils::types::UniquePointer;
  } // namespace

  inline fn GetCacheManager() -> UniquePointer<CacheManager>& {
    static UniquePointer<CacheManager> CacheManager;
    return CacheManager;
  }

  /**
   * @brief Specifies the weather service provider.
   * @see config::DRAC_WEATHER_PROVIDER in `config(.example).hpp`.
   */
  enum class Provider : u8 {
    OPENWEATHERMAP, ///< OpenWeatherMap API. Requires an API key. @see config::DRAC_API_KEY
    OPENMETEO,      ///< OpenMeteo API. Does not require an API key.
    METNO,          ///< Met.no API. Does not require an API key.
  };

  /**
   * @brief Specifies the unit system for weather information.
   * @see config::DRAC_WEATHER_UNIT in `config(.example).hpp`.
   */
  enum class UnitSystem : u8 {
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
    f64            temperature; ///< Degrees (C/F)
    Option<String> name;        ///< Optional town/city name (may be missing for some providers)
    String         description; ///< Weather description (e.g., "clear sky", "rain")
  };

  struct Coords {
    f64 lat;
    f64 lon;
  };

  using Location = std::variant<String, Coords>;

  class IWeatherService {
   public:
    IWeatherService(const IWeatherService&) = delete;
    IWeatherService(IWeatherService&&)      = delete;

    fn operator=(const IWeatherService&)->IWeatherService& = delete;
    fn operator=(IWeatherService&&)->IWeatherService&      = delete;

    virtual ~IWeatherService() = default;

    [[nodiscard]] virtual fn getWeatherInfo() const -> Result<Report> = 0;

   protected:
    IWeatherService() = default;
  };

  fn CreateWeatherService(Provider provider, const Location& location, UnitSystem units, const Option<String>& apiKey = None) -> UniquePointer<IWeatherService>;
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
struct std::formatter<draconis::services::weather::UnitSystem> {
  static constexpr auto parse(std::format_parse_context& ctx) {
    return ctx.begin();
  }

  static fn format(draconis::services::weather::UnitSystem unit, std::format_context& ctx) {
    using matchit::match, matchit::is, matchit::_;

    return std::format_to(ctx.out(), "{}", match(unit)(is | draconis::services::weather::UnitSystem::METRIC = "metric", is | draconis::services::weather::UnitSystem::IMPERIAL = "imperial"));
  }
};

#endif // DRAC_ENABLE_WEATHER
