#define NOMINMAX
#include "weather.hpp"

#include <chrono>                 // std::chrono::{duration, operator-}
#include <curl/curl.h>            // curl_easy_setopt
#include <curl/easy.h>            // curl_easy_init, curl_easy_perform, curl_easy_cleanup
#include <expected>               // std::{expected (Result), unexpected (Err)}
#include <format>                 // std::format
#include <glaze/beve/read.hpp>    // glz::read_beve
#include <glaze/beve/write.hpp>   // glz::write_beve
#include <glaze/core/context.hpp> // glz::{error_ctx, error_code}
#include <glaze/core/opts.hpp>    // glz::opts
#include <glaze/core/reflect.hpp> // glz::format_error
#include <glaze/json/read.hpp>    // NOLINT(misc-include-cleaner) - glaze/json/read.hpp is needed for glz::read<glz::opts>
#include <variant>                // std::{get, holds_alternative}

#include "src/util/cache.hpp"
#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"

#include "config.hpp"

using weather::Output;

namespace {
  using glz::opts, glz::error_ctx, glz::error_code, glz::read, glz::format_error;
  using util::error::DracError, util::error::DracErrorCode;
  using util::types::usize, util::types::Err, util::types::Exception;
  using weather::Coords;
  using namespace util::cache;

  constexpr opts glaze_opts = { .error_on_unknown_keys = false };

  fn WriteCallback(void* contents, const usize size, const usize nmemb, String* str) -> usize {
    const usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  fn MakeApiRequest(const String& url) -> Result<Output> {
    debug_log("Making API request to URL: {}", url);
    CURL*  curl = curl_easy_init();
    String responseBuffer;

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

    Output output;

    if (const error_ctx errc = read<glaze_opts>(output, responseBuffer); errc.ec != error_code::none)
      return Err(DracError(
        DracErrorCode::ParseError, std::format("Failed to parse JSON response: {}", format_error(errc, responseBuffer))
      ));

    return output;
  }
} // namespace

fn Weather::getWeatherInfo() const -> Result<Output> {
  using namespace std::chrono;
  using util::types::i32;

  if (Result<Output> data = ReadCache<Output>("weather")) {
    const Output& dataVal = *data;

    if (const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.dt));
        cacheAge < 60min) // NOLINT(misc-include-cleaner) - inherited from <chrono>
      return dataVal;

    debug_log("Cache expired");
  } else if (data.error().code == DracErrorCode::NotFound) {
    debug_at(data.error());
  } else
    error_at(data.error());

  fn handleApiResult = [](const Result<Output>& result) -> Result<Output> {
    if (!result)
      return Err(result.error());

    if (Result writeResult = WriteCache("weather", *result); !writeResult)
      return Err(writeResult.error());

    return *result;
  };

  if (std::holds_alternative<String>(location)) {
    const auto& city = std::get<String>(location);

    char* escaped = curl_easy_escape(nullptr, city.c_str(), static_cast<i32>(city.length()));

    const String apiUrl =
      std::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", escaped, apiKey, units);

    curl_free(escaped);

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  if (std::holds_alternative<Coords>(location)) {
    const auto& [lat, lon] = std::get<Coords>(location);

    const String apiUrl = std::format(
      "https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, apiKey, units
    );

    return handleApiResult(MakeApiRequest(apiUrl));
  }

  return Err(DracError(DracErrorCode::ParseError, "Invalid location type in configuration."));
}
