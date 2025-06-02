#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include <variant>

#include "Util/ConfigData.hpp"

#include "IWeatherService.hpp"
// clang-format on

namespace weather {
  using util::types::StringView;

  class OpenWeatherMapService final : public IWeatherService {
   public:
    OpenWeatherMapService(std::variant<String, Coords> location, String apiKey, config::WeatherUnit units);
    fn getWeatherInfo() const -> Result<WeatherReport> override;

   private:
    std::variant<String, Coords> m_location;
    String                       m_apiKey;
    config::WeatherUnit          m_units;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER