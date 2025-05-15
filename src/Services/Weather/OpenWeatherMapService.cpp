#define NOMINMAX

#include "OpenWeatherMapService.hpp"

#include <chrono>
#include <curl/curl.h>
#include <curl/easy.h>
#include <format>
#include <glaze/core/meta.hpp>
#include <glaze/json/read.hpp>
#include <matchit.hpp>
#include <variant>

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

using weather::OpenWeatherMapService;
using weather::WeatherReport;

namespace weather {
  using util::types::f64, util::types::i64, util::types::String, util::types::Vec;

  struct OWMResponse {
    struct Main {
      f64 temp;
    } main;

    struct Weather {
      String description;
    };

    Vec<Weather> weather;
    String       name;
    i64          dt;
  };

  struct OWMMainGlaze {
    using T = OWMResponse::Main;

    static constexpr auto value = glz::object("temp", &T::temp);
  };

  struct OWMWeatherGlaze {
    using T = OWMResponse::Weather;

    static constexpr auto value = glz::object("description", &T::description);
  };

  struct OWMResponseGlaze {
    using T = OWMResponse;

    // clang-format off
    static constexpr auto value = glz::object(
      "main", &T::main,
      "weather", &T::weather,
      "name", &T::name,
      "dt", &T::dt
    );
    // clang-format on
  };
} // namespace weather

template <>
struct glz::meta<weather::OWMResponse::Main> : weather::OWMMainGlaze {};

template <>
struct glz::meta<weather::OWMResponse::Weather> : weather::OWMWeatherGlaze {};

template <>
struct glz::meta<weather::OWMResponse> : weather::OWMResponseGlaze {};

namespace {
  using glz::opts, glz::error_ctx, glz::error_code, glz::read, glz::format_error;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::Exception, util::types::Result;
  using namespace util::cache;

  constexpr opts glaze_opts = { .error_on_unknown_keys = false };

  fn WriteCallback(void* contents, const usize size, const usize nmemb, weather::String* str) -> usize {
    const usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn MakeApiRequest(const weather::String& url) -> Result<WeatherReport> {
    CURL*           curl = curl_easy_init();
    weather::String responseBuffer;

    if (!curl)
      return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to initialize cURL"));

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      return Err(DracError(DracErrorCode::ApiUnavailable, std::format("cURL error: {}", curl_easy_strerror(res))));

    weather::OWMResponse owm;
    if (const error_ctx errc = read<glaze_opts>(owm, responseBuffer); errc.ec != error_code::none)
      return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer))));

    WeatherReport report = {
      .temperature = owm.main.temp,
      .name        = owm.name.empty() ? std::nullopt : util::types::Option<std::string>(owm.name),
      .description = !owm.weather.empty() ? owm.weather[0].description : "",
      .timestamp   = static_cast<usize>(owm.dt),
    };

    return report;
  }
} // namespace

OpenWeatherMapService::OpenWeatherMapService(std::variant<String, Coords> location, String apiKey, String units)
  : m_location(std::move(location)), m_apiKey(std::move(apiKey)), m_units(std::move(units)) {}

fn OpenWeatherMapService::getWeatherInfo() const -> Result<WeatherReport> {
  using namespace std::chrono;

  if (Result<WeatherReport> data = ReadCache<WeatherReport>("weather")) {
    const WeatherReport& dataVal = *data;

    if (const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.timestamp)); cacheAge < 60min)
      return dataVal;
  } else {
    using matchit::match, matchit::is, matchit::_;
    using enum DracErrorCode;

    DracError err = data.error();

    match(err.code)(
      is | NotFound = [&] { debug_at(err); },
      is | _        = [&] { error_at(err); }
    );
  }

  fn handleApiResult = [](const Result<WeatherReport>& result) -> Result<WeatherReport> {
    if (!result)
      return Err(result.error());

    if (Result<> writeResult = WriteCache("weather", *result); !writeResult)
      return Err(writeResult.error());

    return *result;
  };

  if (std::holds_alternative<String>(m_location)) {
    using util::types::i32;

    const auto& city = std::get<String>(m_location);

    char* escaped = curl_easy_escape(nullptr, city.c_str(), static_cast<i32>(city.length()));

    const String apiUrl =
      std::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", escaped, m_apiKey, m_units);

    curl_free(escaped);

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  if (std::holds_alternative<Coords>(m_location)) {
    const auto& [lat, lon] = std::get<Coords>(m_location);

    const String apiUrl = std::format(
      "https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, m_apiKey, m_units
    );

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  return util::types::Err(util::error::DracError(util::error::DracErrorCode::ParseError, "Invalid location type in configuration."));
}