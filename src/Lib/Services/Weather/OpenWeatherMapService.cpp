#if DRAC_ENABLE_WEATHER

// clang-format off
#include "OpenWeatherMapService.hpp"

#include <DracUtils/Error.hpp>
#include <DracUtils/Logging.hpp>
#include <DracUtils/Types.hpp>
#include <utility>
#include <variant>

#include "Wrappers/Curl.hpp"

#include "DataTransferObjects.hpp"
#include "Utils/Caching.hpp"
// clang-format on

using namespace util::types;
using util::error::DracError;
using enum util::error::DracErrorCode;
using weather::OpenWeatherMapService;
using weather::Unit;
using weather::WeatherReport;

namespace {
  fn MakeApiRequest(const SZString& url) -> Result<WeatherReport> {
    using glz::error_ctx, glz::read, glz::error_code;
    using util::types::None, util::types::Option;

    SZString responseBuffer;

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

    weather::dto::owm::OWMResponse owmResponse;

    if (const error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(owmResponse, responseBuffer); errc.ec != error_code::none)
      return Err(DracError(ParseError, util::formatting::SzFormat("Failed to parse JSON response: {}", format_error(errc, responseBuffer.data()))));

    if (owmResponse.cod && *owmResponse.cod != 200) {
      using matchit::match, matchit::is, matchit::or_, matchit::_;

      SZString apiErrorMessage = "OpenWeatherMap API error";
      if (owmResponse.message && !owmResponse.message->empty())
        apiErrorMessage += util::formatting::SzFormat(" ({}): {}", *owmResponse.cod, *owmResponse.message);
      else
        apiErrorMessage += util::formatting::SzFormat(" (Code: {})", *owmResponse.cod);

      return Err(DracError(match(*owmResponse.cod)(is | 401 = PermissionDenied, is | 404 = NotFound, is | or_(429, _) = ApiUnavailable), apiErrorMessage));
    }

    WeatherReport report = {
      .temperature = owmResponse.main.temp,
      .name        = owmResponse.name.empty() ? None : Option<SZString>(owmResponse.name),
      .description = !owmResponse.weather.empty() ? owmResponse.weather[0].description : "",
    };

    return report;
  }
} // namespace

OpenWeatherMapService::OpenWeatherMapService(std::variant<SZString, Coords> location, SZString apiKey, Unit units)
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

  if (std::holds_alternative<SZString>(m_location)) {
    const auto& city = std::get<SZString>(m_location);

    Result<SZString> escapedUrl = Curl::Easy::escape(city);
    if (!escapedUrl)
      return Err(escapedUrl.error());

    const SZString apiUrl = util::formatting::SzFormat("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", *escapedUrl, m_apiKey, m_units);

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  if (std::holds_alternative<Coords>(m_location)) {
    const auto& [lat, lon] = std::get<Coords>(m_location);

    const SZString apiUrl = util::formatting::SzFormat("https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, m_apiKey, m_units);

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  return Err(DracError(ParseError, "Invalid location type in configuration."));
}

#endif // DRAC_ENABLE_WEATHER
