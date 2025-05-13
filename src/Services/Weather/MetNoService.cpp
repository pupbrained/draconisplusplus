#define NOMINMAX

#include "MetNoService.hpp"

#include <chrono>              // std::chrono::{system_clock, minutes, seconds}
#include <ctime>               // std::tm, std::timegm
#include <curl/curl.h>         // CURL, CURLcode, CURLOPT_*, CURLE_OK
#include <curl/easy.h>         // curl_easy_init, curl_easy_setopt, curl_easy_perform, curl_easy_strerror, curl_easy_cleanup
#include <format>              // std::format
#include <glaze/json/read.hpp> // glz::read
#include <sstream>             // std::istringstream
#include <unordered_map>       // std::unordered_map

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

using weather::MetNoService;
using weather::WeatherReport;

namespace weather {
  using util::types::f64, util::types::i32, util::types::String, util::types::usize, util::logging::Option;

  struct MetNoTimeseriesDetails {
    f64 airTemperature;
  };

  struct MetNoTimeseriesNext1hSummary {
    String symbolCode;
  };

  struct MetNoTimeseriesNext1h {
    MetNoTimeseriesNext1hSummary summary;
  };

  struct MetNoTimeseriesInstant {
    MetNoTimeseriesDetails details;
  };

  struct MetNoTimeseriesData {
    MetNoTimeseriesInstant        instant;
    Option<MetNoTimeseriesNext1h> next1Hours;
  };

  struct MetNoTimeseries {
    String              time;
    MetNoTimeseriesData data;
  };

  struct MetNoProperties {
    Vec<MetNoTimeseries> timeseries;
  };

  struct MetNoResponse {
    MetNoProperties properties;
  };

  struct MetNoTimeseriesDetailsGlaze {
    using T = MetNoTimeseriesDetails;

    static constexpr auto value = glz::object("air_temperature", &T::airTemperature);
  };

  struct MetNoTimeseriesNext1hSummaryGlaze {
    using T = MetNoTimeseriesNext1hSummary;

    static constexpr auto value = glz::object("symbol_code", &T::symbolCode);
  };

  struct MetNoTimeseriesNext1hGlaze {
    using T = MetNoTimeseriesNext1h;

    static constexpr auto value = glz::object("summary", &T::summary);
  };

  struct MetNoTimeseriesInstantGlaze {
    using T                     = MetNoTimeseriesInstant;
    static constexpr auto value = glz::object("details", &T::details);
  };

  struct MetNoTimeseriesDataGlaze {
    using T = MetNoTimeseriesData;

    // clang-format off
    static constexpr auto value = glz::object(
      "instant", &T::instant,
      "next_1_hours", &T::next1Hours
    );
    // clang-format on
  };

  struct MetNoTimeseriesGlaze {
    using T = MetNoTimeseries;

    // clang-format off
    static constexpr auto value = glz::object(
      "time", &T::time,
      "data", &T::data
    );
    // clang-format on
  };

  struct MetNoPropertiesGlaze {
    using T = MetNoProperties;

    static constexpr auto value = glz::object("timeseries", &T::timeseries);
  };

  struct MetNoResponseGlaze {
    using T = MetNoResponse;

    static constexpr auto value = glz::object("properties", &T::properties);
  };
} // namespace weather

template <>
struct glz::meta<weather::MetNoTimeseriesDetails> : weather::MetNoTimeseriesDetailsGlaze {};
template <>
struct glz::meta<weather::MetNoTimeseriesNext1hSummary> : weather::MetNoTimeseriesNext1hSummaryGlaze {};
template <>
struct glz::meta<weather::MetNoTimeseriesNext1h> : weather::MetNoTimeseriesNext1hGlaze {};
template <>
struct glz::meta<weather::MetNoTimeseriesInstant> : weather::MetNoTimeseriesInstantGlaze {};
template <>
struct glz::meta<weather::MetNoTimeseriesData> : weather::MetNoTimeseriesDataGlaze {};
template <>
struct glz::meta<weather::MetNoTimeseries> : weather::MetNoTimeseriesGlaze {};
template <>
struct glz::meta<weather::MetNoProperties> : weather::MetNoPropertiesGlaze {};
template <>
struct glz::meta<weather::MetNoResponse> : weather::MetNoResponseGlaze {};

namespace {
  using glz::opts;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::String;

  constexpr opts glazeOpts = { .error_on_unknown_keys = false };

  fn SYMBOL_DESCRIPTIONS() -> const std::unordered_map<String, String>& {
    static const std::unordered_map<String, String> MAP = {
      {                    "clearsky_day",               "clear sky" },
      {                  "clearsky_night",               "clear sky" },
      {          "clearsky_polartwilight",               "clear sky" },
      {                          "cloudy",                  "cloudy" },
      {                        "fair_day",                    "fair" },
      {                      "fair_night",                    "fair" },
      {              "fair_polartwilight",                    "fair" },
      {                             "fog",                     "fog" },
      {                       "heavyrain",              "heavy rain" },
      {             "heavyrainandthunder",  "heavy rain and thunder" },
      {            "heavyrainshowers_day",      "heavy rain showers" },
      {          "heavyrainshowers_night",      "heavy rain showers" },
      {  "heavyrainshowers_polartwilight",      "heavy rain showers" },
      {                      "heavysleet",             "heavy sleet" },
      {            "heavysleetandthunder", "heavy sleet and thunder" },
      {           "heavysleetshowers_day",     "heavy sleet showers" },
      {         "heavysleetshowers_night",     "heavy sleet showers" },
      { "heavysleetshowers_polartwilight",     "heavy sleet showers" },
      {                       "heavysnow",              "heavy snow" },
      {             "heavysnowandthunder",  "heavy snow and thunder" },
      {            "heavysnowshowers_day",      "heavy snow showers" },
      {          "heavysnowshowers_night",      "heavy snow showers" },
      {  "heavysnowshowers_polartwilight",      "heavy snow showers" },
      {                       "lightrain",              "light rain" },
      {             "lightrainandthunder",  "light rain and thunder" },
      {            "lightrainshowers_day",      "light rain showers" },
      {          "lightrainshowers_night",      "light rain showers" },
      {  "lightrainshowers_polartwilight",      "light rain showers" },
      {                      "lightsleet",             "light sleet" },
      {            "lightsleetandthunder", "light sleet and thunder" },
      {           "lightsleetshowers_day",     "light sleet showers" },
      {         "lightsleetshowers_night",     "light sleet showers" },
      { "lightsleetshowers_polartwilight",     "light sleet showers" },
      {                       "lightsnow",              "light snow" },
      {             "lightsnowandthunder",  "light snow and thunder" },
      {            "lightsnowshowers_day",      "light snow showers" },
      {          "lightsnowshowers_night",      "light snow showers" },
      {  "lightsnowshowers_polartwilight",      "light snow showers" },
      {                "partlycloudy_day",           "partly cloudy" },
      {              "partlycloudy_night",           "partly cloudy" },
      {      "partlycloudy_polartwilight",           "partly cloudy" },
      {                            "rain",                    "rain" },
      {                  "rainandthunder",        "rain and thunder" },
      {                 "rainshowers_day",            "rain showers" },
      {               "rainshowers_night",            "rain showers" },
      {       "rainshowers_polartwilight",            "rain showers" },
      {                           "sleet",                   "sleet" },
      {                 "sleetandthunder",       "sleet and thunder" },
      {                "sleetshowers_day",           "sleet showers" },
      {              "sleetshowers_night",           "sleet showers" },
      {      "sleetshowers_polartwilight",           "sleet showers" },
      {                            "snow",                    "snow" },
      {                  "snowandthunder",        "snow and thunder" },
      {                 "snowshowers_day",            "snow showers" },
      {               "snowshowers_night",            "snow showers" },
      {       "snowshowers_polartwilight",            "snow showers" },
      {                         "unknown",                 "unknown" }
    };

    return MAP;
  }

  fn WriteCallback(void* contents, usize size, usize nmemb, String* str) -> usize {
    usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn parse_iso8601_to_epoch(const String& iso8601) -> usize {
    std::tm            time = {};
    std::istringstream stream(iso8601);

    stream >> std::get_time(&time, "%Y-%m-%dT%H:%M:%SZ");

    if (stream.fail())
      return 0;

#ifdef _WIN32
    return static_cast<usize>(_mkgmtime(&time));
#else
    return static_cast<usize>(timegm(&time));
#endif
  }
} // namespace

MetNoService::MetNoService(f64 lat, f64 lon, String units)
  : m_lat(lat), m_lon(lon), m_units(std::move(units)) {}

fn MetNoService::getWeatherInfo() const -> util::types::Result<WeatherReport> {
  using glz::error_ctx, glz::error_code, glz::read, glz::format_error;
  using util::cache::ReadCache, util::cache::WriteCache;
  using util::types::String, util::types::Result, util::types::None;

  if (Result<WeatherReport> data = ReadCache<WeatherReport>("weather")) {
    using std::chrono::system_clock, std::chrono::minutes, std::chrono::seconds;

    const WeatherReport& dataVal = *data;

    if (const auto cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.timestamp)); cacheAge < minutes(60))
      return dataVal;
  }

  String url = std::format("https://api.met.no/weatherapi/locationforecast/2.0/compact?lat={:.4f}&lon={:.4f}", m_lat, m_lon);

  CURL* curl = curl_easy_init();

  if (!curl)
    return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to initialize cURL"));

  String responseBuffer;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "draconisplusplus/" DRACONISPLUSPLUS_VERSION " git.pupbrained.xyz/draconisplusplus");

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    return Err(DracError(DracErrorCode::ApiUnavailable, std::format("cURL error: {}", curl_easy_strerror(res))));

  weather::MetNoResponse apiResp {};

  if (error_ctx errc = read<glazeOpts>(apiResp, responseBuffer); errc)
    return Err(DracError(DracErrorCode::ParseError, "Failed to parse met.no JSON response"));

  if (apiResp.properties.timeseries.empty())
    return Err(DracError(DracErrorCode::ParseError, "No timeseries data in met.no response"));

  const MetNoTimeseries& first = apiResp.properties.timeseries.front();

  f64 temp = first.data.instant.details.airTemperature;

  if (m_units == "imperial")
    temp = temp * 9.0 / 5.0 + 32.0;

  String symbolCode  = first.data.next1Hours ? first.data.next1Hours->summary.symbolCode : "";
  String description = symbolCode;

  if (!symbolCode.empty()) {
    auto iter = SYMBOL_DESCRIPTIONS().find(symbolCode);

    if (iter != SYMBOL_DESCRIPTIONS().end())
      description = iter->second;
  }

  WeatherReport out = {
    .temperature = temp,
    .name        = None,
    .description = description,
    .timestamp   = parse_iso8601_to_epoch(first.time),
  };

  if (Result<> writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}
