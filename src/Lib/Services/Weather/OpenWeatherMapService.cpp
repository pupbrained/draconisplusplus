#if DRAC_ENABLE_WEATHER

  #include "OpenWeatherMapService.hpp"

  #include <utility>

  #include "Drac++/Utils/CacheManager.hpp"
  #include "Drac++/Utils/Error.hpp"
  #include "Drac++/Utils/Types.hpp"

  #include "DataTransferObjects.hpp"
  #include "Wrappers/Curl.hpp"

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;
using draconis::services::weather::OpenWeatherMapService;
using draconis::services::weather::Report;
using draconis::services::weather::UnitSystem;

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
      if (const Option<DracError>& initError = curl.getInitializationError())
        ERR_FROM(*initError);

      ERR(ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)");
    }

    if (Result res = curl.perform(); !res)
      ERR_FROM(res.error());

    draconis::services::weather::dto::owm::OWMResponse owmResponse;

    if (const error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(owmResponse, responseBuffer); errc.ec != error_code::none)
      ERR_FMT(ParseError, "Failed to parse JSON response: {}", format_error(errc, responseBuffer.data()));

    if (owmResponse.cod && *owmResponse.cod != 200) {
      using matchit::match, matchit::is, matchit::or_, matchit::_;

      String apiErrorMessage = "OpenWeatherMap API error";

      if (owmResponse.message && !owmResponse.message->empty())
        apiErrorMessage += std::format(" ({}): {}", *owmResponse.cod, *owmResponse.message);
      else
        apiErrorMessage += std::format(" (Code: {})", *owmResponse.cod);

      ERR(
        match(*owmResponse.cod)(is | 401 = PermissionDenied, is | 404 = NotFound, is | or_(429, _) = ApiUnavailable),
        std::move(apiErrorMessage)
      );
    }

    Report report = {
      .temperature = owmResponse.main.temp,
      .name        = owmResponse.name.empty() ? None : Some(owmResponse.name),
      .description = !owmResponse.weather.empty() ? owmResponse.weather[0].description : "",
    };

    return report;
  }
} // namespace

OpenWeatherMapService::OpenWeatherMapService(Location location, String apiKey, const UnitSystem units)
  : m_location(std::move(location)), m_apiKey(std::move(apiKey)), m_units(units) {}

fn OpenWeatherMapService::getWeatherInfo() const -> Result<Report> {
  return GetCacheManager()->getOrSet<Report>(
    "owm_weather",
    [&]() -> Result<Report> {
      if (std::holds_alternative<String>(m_location)) {
        const auto& city = std::get<String>(m_location);

        Result<String> escapedUrl = Curl::Easy::escape(city);
        if (!escapedUrl)
          ERR_FROM(escapedUrl.error());

        const String apiUrl = std::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", *escapedUrl, m_apiKey, m_units);

        return MakeApiRequest(apiUrl);
      }

      if (std::holds_alternative<Coords>(m_location)) {
        const auto& [lat, lon] = std::get<Coords>(m_location);

        const String apiUrl = std::format("https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, m_apiKey, m_units);

        return MakeApiRequest(apiUrl);
      }

      ERR(ParseError, "Invalid location type in configuration.");
    }
  );
}

#endif // DRAC_ENABLE_WEATHER
