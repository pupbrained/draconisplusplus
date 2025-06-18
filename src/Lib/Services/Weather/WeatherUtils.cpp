#if DRAC_ENABLE_WEATHER

  #include "WeatherUtils.hpp"

  #include <charconv> // std::from_chars
  #include <ctime>    // std::tm, timegm, _mkgmtime
  #include <format>   // std::format

  #ifdef __HAIKU__
    #define _DEFAULT_SOURCE // exposes timegm
  #endif

using namespace draconis::utils::types;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace draconis::services::weather::utils {
  fn StripTimeOfDayFromSymbol(const StringView symbol) -> String {
    static constexpr Array<StringView, 3> SUFFIXES = { "_day", "_night", "_polartwilight" };

    for (const StringView& suffix : SUFFIXES)
      if (symbol.size() > suffix.size() && symbol.ends_with(suffix))
        return String(symbol.substr(0, symbol.size() - suffix.size()));

    return String(symbol);
  }

  fn ParseIso8601ToEpoch(StringView iso8601) -> Result<time_t> {
    const usize stringLen = iso8601.size();

    // Supported lengths:
    // 20: "YYYY-MM-DDTHH:MM:SSZ"
    // 16: "YYYY-MM-DDTHH:MM" (seconds assumed 00, UTC assumed)
    if (stringLen != 20 && stringLen != 16)
      return Err(DracError(ParseError, std::format("Failed to parse ISO8601 time \'{}\', unexpected length {}. Expected 16 or 20 characters.", iso8601, stringLen)));

    std::tm timeStruct = {};
    i32     year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0; // Default second to 0

    auto parseInt = [](const StringView sview, i32& out_val) -> bool {
      auto [ptr, ec] = std::from_chars(sview.data(), sview.data() + sview.size(), out_val);
      return ec == std::errc() && ptr == sview.data() + sview.size();
    };

    // Common parsing for YYYY-MM-DDTHH:MM
    // Structure: YYYY-MM-DDTHH:MM
    // Indices:   0123456789012345
    if (!parseInt(iso8601.substr(0, 4), year) || // YYYY
        iso8601[4] != '-' ||
        !parseInt(iso8601.substr(5, 2), month) || // MM
        iso8601[7] != '-' ||
        !parseInt(iso8601.substr(8, 2), day) || // DD
        iso8601[10] != 'T' ||
        !parseInt(iso8601.substr(11, 2), hour) || // HH
        iso8601[13] != ':' ||
        !parseInt(iso8601.substr(14, 2), minute) // MM
    )
      return Err(DracError(ParseError, std::format("Failed to parse common date/time components from ISO8601 string: \'{}\'", iso8601)));

    if (stringLen == 20) // Format: YYYY-MM-DDTHH:MM:SSZ
      if (iso8601[16] != ':' || !parseInt(iso8601.substr(17, 2), second) || iso8601[19] != 'Z')
        return Err(DracError(ParseError, std::format("Failed to parse seconds or UTC zone from 20-character ISO8601 string: \'{}\'", iso8601)));

    timeStruct.tm_year  = year - 1900;
    timeStruct.tm_mon   = month - 1;
    timeStruct.tm_mday  = day;
    timeStruct.tm_hour  = hour;
    timeStruct.tm_min   = minute;
    timeStruct.tm_sec   = second;
    timeStruct.tm_isdst = 0;

  #ifdef _WIN32
    time_t epochTime = _mkgmtime(&timeStruct);

    if (epochTime == -1)
      return Err(DracError(ParseError, "Failed to convert time to epoch using _mkgmtime (invalid date components or out of range)"));

    return static_cast<usize>(epochTime);
  #else
    time_t epochTime = timegm(&timeStruct);

    if (epochTime == static_cast<time_t>(-1))
      return Err(DracError(ParseError, std::format("Failed to convert time to epoch using timegm (invalid date components or out of range)")));

    return epochTime;
  #endif
  }

  fn GetMetnoSymbolDescriptions() -> const std::unordered_map<StringView, StringView>& {
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

  fn GetOpenmeteoWeatherDescription(const i32 code) -> String {
    // Based on WMO Weather interpretation codes (WW)
    // https://open-meteo.com/en/docs
    if (code == 0)
      return "clear sky";
    if (code == 1)
      return "mainly clear";
    if (code == 2)
      return "partly cloudy";
    if (code == 3)
      return "overcast";
    if (code == 45 || code == 48)
      return "fog";
    if (code >= 51 && code <= 55)
      return "drizzle";
    if (code == 56 || code == 57)
      return "freezing drizzle";
    if (code >= 61 && code <= 65)
      return "rain";
    if (code == 66 || code == 67)
      return "freezing rain";
    if (code >= 71 && code <= 75)
      return "snow fall";
    if (code == 77)
      return "snow grains";
    if (code >= 80 && code <= 82)
      return "rain showers";
    if (code == 85 || code == 86)
      return "snow showers";
    if (code == 95)
      return "thunderstorm";
    if (code >= 96 && code <= 99)
      return "thunderstorm with hail";
    return "unknown";
  }

} // namespace draconis::services::weather::utils

#endif // DRAC_ENABLE_WEATHER
