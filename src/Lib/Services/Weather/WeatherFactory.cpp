#include "Drac++/Services/Weather.hpp"

#include "Services/Weather/MetNoService.hpp"
#include "Services/Weather/OpenMeteoService.hpp"
#include "Services/Weather/OpenWeatherMapService.hpp"

namespace draconis::services::weather {
  fn CreateWeatherService(const Provider provider, const Location& location, UnitSystem units, const Option<String>& apiKey) -> UniquePointer<IWeatherService> {
    using enum Provider;

    if (GetCacheManager() == nullptr) {
      GetCacheManager() = std::make_unique<draconis::utils::cache::CacheManager>();
      GetCacheManager()->setGlobalPolicy({ .location = draconis::utils::cache::CacheLocation::Persistent, .ttl = std::chrono::minutes(15) });
    }

    assert(provider == OpenWeatherMap || provider == OpenMeteo || provider == MetNo);
    assert(apiKey.has_value() || provider != OpenWeatherMap);

    switch (provider) {
      case OpenWeatherMap:
        return std::make_unique<OpenWeatherMapService>(location, *apiKey, units);
      case OpenMeteo:
        return std::make_unique<OpenMeteoService>(std::get<Coords>(location).lat, std::get<Coords>(location).lon, units);
      case MetNo:
        return std::make_unique<MetNoService>(std::get<Coords>(location).lat, std::get<Coords>(location).lon, units);
      default:
        std::unreachable();
    }
  }
} // namespace draconis::services::weather
