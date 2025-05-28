#define NOMINMAX

#ifdef __HAIKU__
  #define _DEFAULT_SOURCE // exposes timegm
#endif

#include "MetNoService.hpp"

#include <charconv>
#include <chrono>              // std::chrono::{system_clock, minutes, seconds}
#include <ctime>               // std::tm, std::timegm
#include <curl/curl.h>         // CURL, CURLcode, CURLOPT_*, CURLE_OK
#include <curl/easy.h>         // curl_easy_init, curl_easy_setopt, curl_easy_perform, curl_easy_strerror, curl_easy_cleanup
#include <format>              // std::format
#include <glaze/json/read.hpp> // glz::read
#include <unordered_map>       // std::unordered_map

#include "Util/Caching.hpp"
#include "Util/Error.hpp"
#include "Util/Types.hpp"

using weather::MetNoService;
using weather::WeatherReport;

namespace weather {
  using util::types::f64, util::types::String, util::types::Option;

  struct Details {
    f64 airTemperature;
  };

  struct Next1hSummary {
    String symbolCode;
  };

  struct Next1h {
    Next1hSummary summary;
  };

  struct Instant {
    Details details;
  };

  struct Data {
    Instant        instant;
    Option<Next1h> next1Hours;
  };

  struct Timeseries {
    String time;
    Data   data;
  };

  struct Properties {
    Vec<Timeseries> timeseries;
  };

  struct Response {
    Properties properties;
  };

  struct DetailsG {
    using T = Details;

    static constexpr Object value = glz::object("air_temperature", &T::airTemperature);
  };

  struct Next1hSummaryG {
    using T = Next1hSummary;

    static constexpr Object value = glz::object("symbol_code", &T::symbolCode);
  };

  struct Next1hG {
    using T = Next1h;

    static constexpr Object value = glz::object("summary", &T::summary);
  };

  struct InstantG {
    using T = Instant;

    static constexpr Object value = glz::object("details", &T::details);
  };

  struct DataG {
    using T = Data;

    // clang-format off
    static constexpr Object value = glz::object(
      "instant",      &T::instant,
      "next_1_hours", &T::next1Hours
    );
    // clang-format on
  };

  struct TimeseriesG {
    using T = Timeseries;

    // clang-format off
    static constexpr Object value = glz::object(
      "time", &T::time,
      "data", &T::data
    );
    // clang-format on
  };

  struct PropertiesG {
    using T = Properties;

    static constexpr Object value = glz::object("timeseries", &T::timeseries);
  };

  struct ResponseG {
    using T = Response;

    static constexpr Object value = glz::object("properties", &T::properties);
  };
} // namespace weather

namespace glz {
  using weather::DetailsG, weather::Next1hSummaryG, weather::Next1hG, weather::InstantG, weather::DataG, weather::TimeseriesG, weather::PropertiesG, weather::ResponseG;
  using weather::Timeseries, weather::Details, weather::Next1hSummary, weather::Next1h, weather::Instant, weather::Data, weather::Properties, weather::Response;

  template <>
  struct meta<Details> : DetailsG {};
  template <>
  struct meta<Next1hSummary> : Next1hSummaryG {};
  template <>
  struct meta<Next1h> : Next1hG {};
  template <>
  struct meta<Instant> : InstantG {};
  template <>
  struct meta<Data> : DataG {};
  template <>
  struct meta<Timeseries> : TimeseriesG {};
  template <>
  struct meta<Properties> : PropertiesG {};
  template <>
  struct meta<Response> : ResponseG {};
} // namespace glz

namespace {
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::String, util::types::StringView, util::types::Result;

  fn WriteCallback(void* contents, const usize size, const usize nmemb, String* str) -> usize {
    const usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn SYMBOL_DESCRIPTIONS() -> const std::unordered_map<StringView, StringView>& {
    static const std::unordered_map<StringView, StringView> MAP = {
      // Clear / Fair
      {             "clearsky",               "clear sky" },
      {                 "fair",                    "fair" },
      {         "partlycloudy",           "partly cloudy" },
      {               "cloudy",                  "cloudy" },
      {                  "fog",                     "fog" },

      // Rain
      {            "lightrain",              "light rain" },
      {     "lightrainshowers",      "light rain showers" },
      {  "lightrainandthunder",  "light rain and thunder" },
      {                 "rain",                    "rain" },
      {          "rainshowers",            "rain showers" },
      {       "rainandthunder",        "rain and thunder" },
      {            "heavyrain",              "heavy rain" },
      {     "heavyrainshowers",      "heavy rain showers" },
      {  "heavyrainandthunder",  "heavy rain and thunder" },

      // Sleet
      {           "lightsleet",             "light sleet" },
      {    "lightsleetshowers",     "light sleet showers" },
      { "lightsleetandthunder", "light sleet and thunder" },
      {                "sleet",                   "sleet" },
      {         "sleetshowers",           "sleet showers" },
      {      "sleetandthunder",       "sleet and thunder" },
      {           "heavysleet",             "heavy sleet" },
      {    "heavysleetshowers",     "heavy sleet showers" },
      { "heavysleetandthunder", "heavy sleet and thunder" },

      // Snow
      {            "lightsnow",              "light snow" },
      {     "lightsnowshowers",      "light snow showers" },
      {  "lightsnowandthunder",  "light snow and thunder" },
      {                 "snow",                    "snow" },
      {          "snowshowers",            "snow showers" },
      {       "snowandthunder",        "snow and thunder" },
      {            "heavysnow",              "heavy snow" },
      {     "heavysnowshowers",      "heavy snow showers" },
      {  "heavysnowandthunder",  "heavy snow and thunder" },
    };

    return MAP;
  }

  fn strip_time_of_day(const StringView& symbol) -> StringView {
    using util::types::Array, util::types::StringView;

    static constexpr Array<StringView, 3> SUFFIXES = { "_day", "_night", "_polartwilight" };

    for (const StringView& suffix : SUFFIXES)
      if (symbol.size() > suffix.size() && symbol.ends_with(suffix))
        return symbol.substr(0, symbol.size() - suffix.size());

    return symbol;
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

MetNoService::MetNoService(const f64 lat, const f64 lon, String units)
  : m_lat(lat), m_lon(lon), m_units(std::move(units)) {}

fn MetNoService::getWeatherInfo() const -> Result<WeatherReport> {
  using glz::error_ctx, glz::read, glz::error_code;
  using util::cache::ReadCache, util::cache::WriteCache;
  using util::types::None;

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

  CURL* curl = curl_easy_init();

  if (!curl)
    return Err(DracError(DracErrorCode::ApiUnavailable, "Failed to initialize cURL"));

  String responseBuffer;
  curl_easy_setopt(curl, CURLOPT_URL, std::format("https://api.met.no/weatherapi/locationforecast/2.0/compact?lat={:.4f}&lon={:.4f}", m_lat, m_lon).c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "draconisplusplus/" DRACONISPLUSPLUS_VERSION " git.pupbrained.xyz/draconisplusplus");

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK)
    return Err(DracError(DracErrorCode::ApiUnavailable, std::format("cURL error: {}", curl_easy_strerror(res))));

  weather::Response apiResp {};

  if (error_ctx errc = read<glz::opts { .error_on_unknown_keys = false }>(apiResp, responseBuffer); errc.ec != error_code::none)
    return Err(DracError(DracErrorCode::ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer))));

  if (apiResp.properties.timeseries.empty())
    return Err(DracError(DracErrorCode::ParseError, "No timeseries data in met.no response"));

  const auto& [time, data] = apiResp.properties.timeseries.front();

  f64 temp = data.instant.details.airTemperature;

  if (m_units == "imperial")
    temp = temp * 9.0 / 5.0 + 32.0;

  String symbolCode = data.next1Hours ? data.next1Hours->summary.symbolCode : "";

  if (!symbolCode.empty())
    if (auto iter = SYMBOL_DESCRIPTIONS().find(strip_time_of_day(symbolCode)); iter != SYMBOL_DESCRIPTIONS().end())
      symbolCode = iter->second;

  Result<usize> timestamp = parse_iso8601_to_epoch(time);

  if (!timestamp)
    return Err(timestamp.error());

  WeatherReport out = {
    .temperature = temp,
    .name        = None,
    .description = std::move(symbolCode),
    .timestamp   = *timestamp,
  };

  if (Result writeResult = WriteCache("weather", out); !writeResult)
    return Err(writeResult.error());

  return out;
}
