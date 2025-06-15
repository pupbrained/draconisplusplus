#pragma once

#if DRAC_ENABLE_WEATHER

  #include "Drac++/Services/Weather.hpp"

  #include "DracUtils/Types.hpp"

namespace weather {
  class OpenMeteoService final : public IWeatherService {
   public:
    OpenMeteoService(util::types::f64 lat, util::types::f64 lon, Unit units);
    [[nodiscard]] fn getWeatherInfo() const -> util::types::Result<WeatherReport> override;

   private:
    util::types::f64 m_lat;
    util::types::f64 m_lon;
    Unit             m_units;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER