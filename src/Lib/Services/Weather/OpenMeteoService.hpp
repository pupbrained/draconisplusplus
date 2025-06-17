#pragma once

#if DRAC_ENABLE_WEATHER

  #include "Drac++/Services/Weather.hpp"

  #include "DracUtils/Types.hpp"

namespace weather {
  class OpenMeteoService final : public IWeatherService {
   public:
    OpenMeteoService(drac::types::f64 lat, drac::types::f64 lon, Unit units);
    [[nodiscard]] fn getWeatherInfo() const -> drac::types::Result<Report> override;

   private:
    drac::types::f64 m_lat;
    drac::types::f64 m_lon;
    Unit             m_units;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER
