#pragma once

#include "IWeatherService.hpp"

namespace weather {
  class OpenMeteoService : public IWeatherService {
   public:
    OpenMeteoService(f64 lat, f64 lon, String units = "metric");
    [[nodiscard]] fn getWeatherInfo() const -> Result<WeatherReport> override;

   private:
    double m_lat;
    double m_lon;
    String m_units;
  };
} // namespace weather