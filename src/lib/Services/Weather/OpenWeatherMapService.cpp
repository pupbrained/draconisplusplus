#if DRAC_ENABLE_WEATHER

// clang-format off
#include "OpenWeatherMapService.hpp"

#include <format>
#include <utility>
#include <variant>

#include "Services/Weather/DataTransferObjects.hpp"

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "Wrappers/Curl.hpp"
// clang-format on

using weather::OpenWeatherMapService;
using weather::WeatherReport;
// DTOs and their Glaze meta definitions are now in Services/Weather/DataTransferObjects.hpp

namespace {
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::Result, util::types::String, util::types::StringView;

  fn MakeApiRequest(const String& url) -> Result<WeatherReport> {
    using glz::error_ctx, glz::read, glz::error_code;
    using util::types::None, util::types::Option;

    String responseBuffer;

    // clang-format off
    Curl::Easy curl({
      .url = url,
      .writeBuffer = &responseBuffer,
      .timeoutSecs = 10L,
      .connectTimeoutSecs = 5L
    });
    // clang-format on

    if (!curl) {
      if (Option<DracError> initError = curl.getInitializationError())
        return Err(*initError);
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to initialize cURL (Easy handle is invalid after construction)"));
    }

    if (Result res = curl.perform(); !res)
      return Err(res.error());

    weather::dto::owm::OWMResponse owmResponse;

    if (const error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(owmResponse, responseBuffer); errc.ec != error_code::none)
      return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer))));

    // Check for OpenWeatherMap API error codes
    if (owmResponse.cod && *owmResponse.cod != 200) {
      String apiErrorMessage = "OpenWeatherMap API error";
      if (owmResponse.message && !owmResponse.message->empty()) {
        apiErrorMessage += std::format(" ({}): {}", *owmResponse.cod, *owmResponse.message);
      } else {
        apiErrorMessage += std::format(" (Code: {})", *owmResponse.cod);
      }
      // Map to specific DracErrorCodes if desired
      DracErrorCode dErrorCode = DracErrorCode::ApiUnavailable; // General API issue
      if (*owmResponse.cod == 401)
        dErrorCode = DracErrorCode::PermissionDenied; // Authentication error
      else if (*owmResponse.cod == 404)
        dErrorCode = DracErrorCode::NotFound; // Location not found
      else if (*owmResponse.cod == 429)
        dErrorCode = DracErrorCode::ApiUnavailable; // Rate limited, treat as temporarily unavailable

      return Err(DracError(dErrorCode, apiErrorMessage));
    }

    WeatherReport report = {
      .temperature = owmResponse.main.temp,
      .name        = owmResponse.name.empty() ? None : Option<String>(owmResponse.name),
      .description = !owmResponse.weather.empty() ? owmResponse.weather[0].description : "",
    };

    return report;
  }
} // namespace

OpenWeatherMapService::OpenWeatherMapService(std::variant<String, Coords> location, String apiKey, config::WeatherUnit units)
  : m_location(std::move(location)), m_apiKey(std::move(apiKey)), m_units(units) {}

fn OpenWeatherMapService::getWeatherInfo() const -> Result<WeatherReport> {
  using util::cache::GetValidCache, util::cache::WriteCache;

  if (Result<WeatherReport> cachedDataResult = GetValidCache<WeatherReport>("weather"))
    return *cachedDataResult;
  else
    debug_at(cachedDataResult.error());

  fn handleApiResult = [](const Result<WeatherReport>& result) -> Result<WeatherReport> {
    if (!result)
      return Err(result.error());

    if (Result writeResult = WriteCache("weather", *result); !writeResult)
      return Err(writeResult.error());

    return *result;
  };

  if (std::holds_alternative<String>(m_location)) {
    using util::types::i32;

    const auto& city = std::get<String>(m_location);

    Result<String> escapedUrl = Curl::Easy::escape(city);
    if (!escapedUrl)
      return Err(escapedUrl.error());

    const String apiUrl = std::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", *escapedUrl, m_apiKey, m_units);

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  if (std::holds_alternative<Coords>(m_location)) {
    const auto& [lat, lon] = std::get<Coords>(m_location);

    const String apiUrl = std::format("https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, m_apiKey, m_units);

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  return Err(DracError(DracErrorCode::ParseError, "Invalid location type in configuration."));
}

#endif // DRAC_ENABLE_WEATHER
