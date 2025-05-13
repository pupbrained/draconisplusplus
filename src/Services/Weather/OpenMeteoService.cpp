#define NOMINMAX

#ifdef __HAIKU__
  #define _DEFAULT_SOURCE
#endif

#include "OpenMeteoService.hpp"

#include <chrono>              // std::chrono::{system_clock, minutes, seconds}
#include <ctime>               // std::tm, std::timegm
#include <curl/curl.h>         // CURL, CURLcode, CURLOPT_*, CURLE_OK
#include <curl/easy.h>         // curl_easy_init, curl_easy_setopt, curl_easy_perform, curl_easy_strerror, curl_easy_cleanup
#include <format>              // std::format
#include <glaze/json/read.hpp> // glz::read
#include <sstream>             // std::istringstream

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

using weather::OpenMeteoService;
using weather::WeatherReport;

namespace weather {
  using util::types::f64, util::types::i32, util::types::String;

  struct OpenMeteoResponse {
    struct CurrentWeather {
      f64    temperature;
      i32    weathercode;
      String time;
    } currentWeather;
  };

  struct OpenMeteoGlaze {
    using T = OpenMeteoResponse;

    // clang-format off
    static constexpr auto value = glz::object(
      "current_weather", &T::currentWeather
    );
    // clang-format on
  };

  struct CurrentWeatherGlaze {
    using T = OpenMeteoResponse::CurrentWeather;

    // clang-format off
    static constexpr auto value = glz::object(
      "temperature", &T::temperature,
      "weathercode", &T::weathercode,
      "time", &T::time
    );
    // clang-format on
  };
} // namespace weather

template <>
struct glz::meta<weather::OpenMeteoResponse> : weather::OpenMeteoGlaze {};

template <>
struct glz::meta<weather::OpenMeteoResponse::CurrentWeather> : weather::CurrentWeatherGlaze {};

namespace {
  using glz::opts;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::String;

  constexpr opts glazeOpts = { .error_on_unknown_keys = false };

  fn WriteCallback(void* contents, usize size, usize nmemb, String* str) -> usize {
    usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn parse_iso8601_to_epoch(const String& iso8601) -> usize {
    std::tm            time = {};
    std::istringstream stream(iso8601);
    stream >> std::get_time(&time, "%Y-%m-%dT%H:%M");
    if (stream.fail())
      return 0;
#ifdef _WIN32
    return static_cast<usize>(_mkgmtime(&time));
#else
    return static_cast<usize>(timegm(&time));
#endif
  }
} // namespace

OpenMeteoService::OpenMeteoService(f64 lat, f64 lon, String units)
  : m_lat(lat), m_lon(lon), m_units(std::move(units)) {}

fn OpenMeteoService::getWeatherInfo() const -> util::types::Result<WeatherReport> {
  using glz::error_ctx, glz::error_code, glz::read, glz::format_error;
  using util::cache::ReadCache, util::cache::WriteCache;
  using util::types::Array, util::types::String, util::types::Result, util::types::None;

  if (Result<WeatherReport> data = ReadCache<WeatherReport>("weather")) {
    using std::chrono::system_clock, std::chrono::minutes, std::chrono::seconds;

    const WeatherReport& dataVal = *data;

    if (const auto cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.timestamp)); cacheAge < minutes(60))
      return dataVal;
  }

  String url = std::format(
    "https://api.open-meteo.com/v1/forecast?latitude={:.4f}&longitude={:.4f}&current_weather=true&temperature_unit={}",
    m_lat,
    m_lon,
    m_units == "imperial" ? "fahrenheit" : "celsius"
  );

  CURL* curl = curl_easy_init();
  if (!curl)
    return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to initialize cURL"));

  String responseBuffer;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    return Err(DracError(DracErrorCode::ApiUnavailable, std::format("cURL error: {}", curl_easy_strerror(res))));

  OpenMeteoResponse apiResp {};

  if (error_ctx errc = read<glazeOpts>(apiResp, responseBuffer); errc)
    return Err(DracError(DracErrorCode::ParseError, "Failed to parse Open-Meteo JSON response"));

  static constexpr Array<const char*, 9> CODE_DESC = {
    "clear sky",
    "mainly clear",
    "partly cloudy",
    "overcast",
    "fog",
    "drizzle",
    "rain",
    "snow",
    "thunderstorm"
  };

  WeatherReport out = {
    .temperature = apiResp.currentWeather.temperature,
    .name        = None,
    .description = CODE_DESC.at(apiResp.currentWeather.weathercode),
    .timestamp   = parse_iso8601_to_epoch(apiResp.currentWeather.time),
  };

  if (Result<> writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}
