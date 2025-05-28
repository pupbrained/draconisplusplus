#define NOMINMAX

#include "OpenMeteoService.hpp"

#ifdef __HAIKU__
  #define _DEFAULT_SOURCE // exposes timegm
#endif

#include <chrono>              // std::chrono::{system_clock, minutes, seconds}
#include <ctime>               // std::tm, std::timegm
#include <curl/curl.h>         // CURL, CURLcode, CURLOPT_*, CURLE_OK
#include <curl/easy.h>         // curl_easy_init, curl_easy_setopt, curl_easy_perform, curl_easy_strerror, curl_easy_cleanup
#include <format>              // std::format
#include <glaze/json/read.hpp> // glz::read

#include "Services/Weather.hpp"

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

using weather::OpenMeteoService;
using weather::WeatherReport;

namespace weather {
  using util::types::f64, util::types::i32, util::types::String;

  struct Response {
    struct Current {
      f64    temperature;
      i32    weathercode;
      String time;
    } currentWeather;
  };

  struct ResponseG {
    using T = Response;

    static constexpr auto value = glz::object("current_weather", &T::currentWeather);
  };

  struct CurrentG {
    using T = Response::Current;

    // clang-format off
    static constexpr auto value = glz::object(
      "temperature", &T::temperature,
      "weathercode", &T::weathercode,
      "time",        &T::time
    );
    // clang-format on
  };
} // namespace weather

namespace glz {
  using weather::Response, weather::ResponseG, weather::CurrentG;

  template <>
  struct meta<Response> : ResponseG {};
  template <>
  struct meta<Response::Current> : CurrentG {};
} // namespace glz

namespace {
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::String, util::types::Result, util::types::StringView;

  fn WriteCallback(void* contents, const usize size, const usize nmemb, String* str) -> usize {
    const usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn parse_iso8601_to_epoch(const StringView iso8601) -> Result<usize> {
    using util::types::i32;

    if (iso8601.size() != 20)
      return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse ISO8601 time, expected 20 characters, got {}", iso8601.size())));

    std::tm time = {};

    i32 year = 0, mon = 0, mday = 0, hour = 0, min = 0, sec = 0;

    fn parseInt = [](StringView sview, i32& out) -> bool {
      auto [ptr, ec] = std::from_chars(sview.data(), sview.data() + sview.size(), out);

      return ec == std::errc() && ptr == sview.data() + sview.size();
    };

    if (!parseInt(iso8601.substr(0, 4), year) ||
        !parseInt(iso8601.substr(5, 2), mon) ||
        !parseInt(iso8601.substr(8, 2), mday) ||
        !parseInt(iso8601.substr(11, 2), hour) ||
        !parseInt(iso8601.substr(14, 2), min) ||
        !parseInt(iso8601.substr(17, 2), sec)) {
      return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse ISO8601 time: {}", String(iso8601))));
    }

    time.tm_year = year - 1900;
    time.tm_mon  = mon - 1;
    time.tm_mday = mday;
    time.tm_hour = hour;
    time.tm_min  = min;
    time.tm_sec  = sec;

#ifdef _WIN32
    return static_cast<usize>(_mkgmtime(&time));
#else
    return static_cast<usize>(timegm(&time));
#endif
  }
} // namespace

OpenMeteoService::OpenMeteoService(const f64 lat, const f64 lon, String units)
  : m_lat(lat), m_lon(lon), m_units(std::move(units)) {}

fn OpenMeteoService::getWeatherInfo() const -> Result<WeatherReport> {
  using glz::error_ctx, glz::read, glz::error_code;
  using util::cache::ReadCache, util::cache::WriteCache;
  using util::types::Array, util::types::None, util::types::StringView;

  if (Result<WeatherReport> data = ReadCache<WeatherReport>("weather")) {
    using std::chrono::system_clock, std::chrono::minutes, std::chrono::seconds, std::chrono::duration;

    const WeatherReport& dataVal = *data;

    if (const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.timestamp)); cacheAge < minutes(60))
      return dataVal;
  } else {
    if (const DracError& err = data.error(); err.code == DracErrorCode::NotFound)
      debug_at(err);
    else
      error_at(err);
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

  Response apiResp {};

  if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer); errc.ec != error_code::none)
    return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer))));

  static constexpr Array<StringView, 9> CODE_DESC = {
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

  Result<usize> timestamp = parse_iso8601_to_epoch(apiResp.currentWeather.time);

  if (!timestamp)
    return Err(timestamp.error());

  WeatherReport out = {
    .temperature = apiResp.currentWeather.temperature,
    .name        = None,
    .description = String(CODE_DESC.at(apiResp.currentWeather.weathercode)),
    .timestamp   = *timestamp,
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}
