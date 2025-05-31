#define NOMINMAX

#include "OpenWeatherMapService.hpp"

#include <chrono>
#include <format>
// <glaze/core/meta.hpp> and <glaze/json/read.hpp> are included via DataTransferObjects.hpp
#include <utility>
#include <variant>

#include "Services/Weather/DataTransferObjects.hpp"
#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "Wrappers/Curl.hpp"

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

    WeatherReport report = {
      .temperature = owmResponse.main.temp,
      .name        = owmResponse.name.empty() ? None : Option<String>(owmResponse.name),
      .description = !owmResponse.weather.empty() ? owmResponse.weather[0].description : "",
      .timestamp   = static_cast<usize>(owmResponse.dt),
    };

    return report;
  }
} // namespace

OpenWeatherMapService::OpenWeatherMapService(std::variant<String, Coords> location, String apiKey, String units)
  : m_location(std::move(location)), m_apiKey(std::move(apiKey)), m_units(std::move(units)) {}

fn OpenWeatherMapService::getWeatherInfo() const -> Result<WeatherReport> {
  using util::cache::ReadCache, util::cache::WriteCache;

  if (Result<WeatherReport> data = ReadCache<WeatherReport>("weather")) {
    using std::chrono::system_clock, std::chrono::seconds, std::chrono::minutes, std::chrono::duration;

    const WeatherReport& dataVal = *data;

    if (const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.timestamp)); cacheAge < minutes(60))
      return dataVal;
  } else {
    if (const DracError& err = data.error(); err.code == DracErrorCode::NotFound)
      debug_at(err);
    else
      error_at(err);
  }

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
