#pragma once

#if DRAC_ENABLE_WEATHER

// clang-format off
#include "Util/ConfigData.hpp"

#include "IWeatherService.hpp"
// clang-format on

namespace weather {
  class MetNoService final : public IWeatherService {
   public:
    MetNoService(f64 lat, f64 lon, config::WeatherUnit units);
    [[nodiscard]] fn getWeatherInfo() const -> Result<WeatherReport> override;

   private:
    f64                 m_lat;
    f64                 m_lon;
    config::WeatherUnit m_units;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER