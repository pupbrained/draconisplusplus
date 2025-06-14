#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include <variant>

#include "Drac++/Services/Weather.hpp"
#include "DracUtils/Types.hpp"
// clang-format on

namespace weather {
  class OpenWeatherMapService final : public IWeatherService {
   public:
    OpenWeatherMapService(std::variant<util::types::String, Coords> location, util::types::String apiKey, Unit units);
    fn getWeatherInfo() const -> util::types::Result<WeatherReport> override;

   private:
    std::variant<util::types::String, Coords> m_location;
    util::types::String                       m_apiKey;
    Unit                                      m_units;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER