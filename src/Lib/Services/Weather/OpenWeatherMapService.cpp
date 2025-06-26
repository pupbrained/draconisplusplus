#if DRAC_ENABLE_WEATHER

  #include "OpenWeatherMapService.hpp"

  #include <utility>

  #include "Drac++/Utils/CacheManager.hpp"
  #include "Drac++/Utils/Error.hpp"
  #include "Drac++/Utils/Logging.hpp"
  #include "Drac++/Utils/Types.hpp"

  #include "DataTransferObjects.hpp"
  #include "Wrappers/Curl.hpp"

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;
using draconis::services::weather::OpenWeatherMapService;
using draconis::services::weather::Report;
using draconis::services::weather::Unit;
using draconis::services::weather::s_cacheManager;

namespace {
  fn MakeApiRequest(const String& url) -> Result<Report> {
    using draconis::utils::types::None, draconis::utils::types::Option;
    using glz::error_ctx, glz::read, glz::error_code;

    String responseBuffer;

    Curl::Easy curl({
      .url                = url,
      .writeBuffer        = &responseBuffer,
      .timeoutSecs        = 10L,
      .connectTimeoutSecs = 5L,
    });

    if (!curl) {
      if (Option<DracError> initError = curl.getInitializationError())
        return Err(*initError);
      return Err(DracError(ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)"));
    }

    if (Result res = curl.perform(); !res)
      return Err(res.error());

    draconis::services::weather::dto::owm::OWMResponse owmResponse;

    if (const error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(owmResponse, responseBuffer); errc.ec != error_code::none)
      return Err(DracError(ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer.data()))));

    if (owmResponse.cod && *owmResponse.cod != 200) {
      using matchit::match, matchit::is, matchit::or_, matchit::_;

      String apiErrorMessage = "OpenWeatherMap API error";
      if (owmResponse.message && !owmResponse.message->empty())
        apiErrorMessage += std::format(" ({}): {}", *owmResponse.cod, *owmResponse.message);
      else
        apiErrorMessage += std::format(" (Code: {})", *owmResponse.cod);

      return Err(DracError(match(*owmResponse.cod)(is | 401 = PermissionDenied, is | 404 = NotFound, is | or_(429, _) = ApiUnavailable), apiErrorMessage));
    }

    Report report = {
      .temperature = owmResponse.main.temp,
      .name        = owmResponse.name.empty() ? None : Option<String>(owmResponse.name),
      .description = !owmResponse.weather.empty() ? owmResponse.weather[0].description : "",
    };

    return report;
  }
} // namespace

OpenWeatherMapService::OpenWeatherMapService(Location location, String apiKey, const Unit units)
  : m_location(std::move(location)), m_apiKey(std::move(apiKey)), m_units(units) {}

fn OpenWeatherMapService::getWeatherInfo() const -> Result<Report> {
  String cacheKey;
  if (std::holds_alternative<String>(m_location)) {
    cacheKey = std::format("owm_weather_city_{}", std::get<String>(m_location));
  } else if (std::holds_alternative<Coords>(m_location)) {
    const auto& [lat, lon] = std::get<Coords>(m_location);
    cacheKey               = std::format("owm_weather_coords_{:.3f}_{:.3f}", lat, lon);
  } else {
    return Err(DracError(ParseError, "Invalid location type in configuration."));
  }

  return s_cacheManager->getOrSet<Report>(
    cacheKey,
    [&]() -> Result<Report> {
      if (std::holds_alternative<String>(m_location)) {
        const auto& city = std::get<String>(m_location);

        Result<String> escapedUrl = Curl::Easy::escape(city);
        if (!escapedUrl)
          return Err(escapedUrl.error());

        const String apiUrl = std::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", *escapedUrl, m_apiKey, m_units);

        return MakeApiRequest(apiUrl);
      }

      if (std::holds_alternative<Coords>(m_location)) {
        const auto& [lat, lon] = std::get<Coords>(m_location);

        const String apiUrl = std::format("https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, m_apiKey, m_units);

        return MakeApiRequest(apiUrl);
      }

      return Err(DracError(ParseError, "Invalid location type in configuration."));
    }
  );
}

#endif // DRAC_ENABLE_WEATHER
