#pragma once

#include "IWeatherService.hpp"

namespace weather {
  class OpenMeteoService final : public IWeatherService {
   public:
    OpenMeteoService(f64 lat, f64 lon, String units = "metric");
    [[nodiscard]] fn getWeatherInfo() const -> Result<WeatherReport> override;

   private:
    f64    m_lat;
    f64    m_lon;
    String m_units;
  };
} // namespace weather