#pragma once

#if DRAC_ENABLE_WEATHER

  #include "Drac++/Services/Weather.hpp"

  #include "DracUtils/Types.hpp"

namespace weather {
  class OpenWeatherMapService final : public IWeatherService {
   public:
    OpenWeatherMapService(Location location, util::types::String apiKey, Unit units);
    fn getWeatherInfo() const -> util::types::Result<Report> override;

   private:
    Location            m_location;
    util::types::String m_apiKey;
    Unit                m_units;
  };
} // namespace weather

#endif // DRAC_ENABLE_WEATHER
