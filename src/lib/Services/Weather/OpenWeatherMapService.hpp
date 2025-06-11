#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include <variant>

#include "Util/ConfigData.hpp"

#include "IWeatherService.hpp"
// clang-format on

namespace weather {
  class OpenWeatherMapService final : public IWeatherService {
   public:
    OpenWeatherMapService(std::variant<util::types::String, Coords> location, util::types::String apiKey, config::WeatherUnit units);
    fn getWeatherInfo() const -> util::types::Result<WeatherReport> override;

   private:
    std::variant<util::types::String, Coords> m_location;
    util::types::String                       m_apiKey;
    config::WeatherUnit                       m_units;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER