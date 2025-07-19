#pragma once

#if DRAC_ENABLE_WEATHER

  #include "Drac++/Services/Weather.hpp"

  #include "Drac++/Utils/Types.hpp"

namespace draconis::services::weather {
  class OpenWeatherMapService final : public IWeatherService {
   public:
    OpenWeatherMapService(Location location, utils::types::String apiKey, UnitSystem units);
    fn getWeatherInfo() const -> utils::types::Result<Report> override;

   private:
    Location             m_location;
    utils::types::String m_apiKey;
    UnitSystem           m_units;
  };
} // namespace draconis::services::weather

#endif // DRAC_ENABLE_WEATHER
