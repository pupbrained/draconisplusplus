#include <chrono>
#include <curl/curl.h>
#include <expected>
#include <filesystem>
#include <fmt/core.h>
#include <rfl/json.hpp>
#include <rfl/json/load.hpp>

#include "config.h"

namespace fs = std::filesystem;
using namespace std::string_literals;

// Alias for cleaner error handling
template <typename T>
using Result = std::expected<T, std::string>;

namespace {
  // Common function to get cache path
  fn GetCachePath() -> Result<fs::path> {
    std::error_code errc;
    fs::path        cachePath = fs::temp_directory_path(errc);

    if (errc)
      return std::unexpected("Failed to get temp directory: "s + errc.message());

    cachePath /= "weather_cache.json";
    return cachePath;
  }

  // Function to read cache from file
  fn ReadCacheFromFile() -> Result<WeatherOutput> {
    Result<fs::path> cachePath = GetCachePath();
    if (!cachePath)
      return std::unexpected(cachePath.error());

    std::ifstream ifs(*cachePath, std::ios::binary);
    if (!ifs.is_open())
      return std::unexpected("Cache file not found: "s + cachePath->string());

    DEBUG_LOG("Reading from cache file...");

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    rfl::Result<WeatherOutput> result = rfl::json::read<WeatherOutput>(content);
    if (!result)
      return std::unexpected(result.error().what());

    DEBUG_LOG("Successfully read from cache file.");
    return *result;
  }

  // Function to write cache to file
  fn WriteCacheToFile(const WeatherOutput& data) -> Result<void> {
    Result<fs::path> cachePath = GetCachePath();
    if (!cachePath)
      return std::unexpected(cachePath.error());

    DEBUG_LOG("Writing to cache file...");

    fs::path tempPath = *cachePath;
    tempPath += ".tmp";

    {
      std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
      if (!ofs.is_open())
        return std::unexpected("Failed to open temp file: "s + tempPath.string());

      std::string json = rfl::json::write(data);
      ofs << json;

      if (!ofs)
        return std::unexpected("Failed to write to temp file");
    }

    std::error_code errc;
    fs::rename(tempPath, *cachePath, errc);

    if (errc) {
      fs::remove(tempPath, errc);
      return std::unexpected("Failed to replace cache file: "s + errc.message());
    }

    DEBUG_LOG("Successfully wrote to cache file.");
    return {};
  }

  fn WriteCallback(void* contents, size_t size, size_t nmemb, std::string* str) -> size_t {
    const size_t totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  // Function to make API request
  fn MakeApiRequest(const std::string& url) -> Result<WeatherOutput> {
    DEBUG_LOG("Making API request to URL: {}", url);

    CURL*       curl = curl_easy_init();
    std::string responseBuffer;

    if (!curl)
      return std::unexpected("Failed to initialize cURL");

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
      return std::unexpected(fmt::format("cURL error: {}", curl_easy_strerror(res)));

    DEBUG_LOG("API response size: {}", responseBuffer.size());

    rfl::Result<WeatherOutput> output = rfl::json::read<WeatherOutput>(responseBuffer);
    if (!output)
      return std::unexpected(output.error().what());

    return *output;
  }
}

// Core function to get weather information
fn Weather::getWeatherInfo() const -> WeatherOutput {
  using namespace std::chrono;

  if (Result<WeatherOutput> data = ReadCacheFromFile()) {
    const WeatherOutput&   dataVal  = *data;
    const duration<double> cacheAge = system_clock::now() - system_clock::time_point(seconds(dataVal.dt));

    if (cacheAge < 10min) {
      DEBUG_LOG("Using valid cache");
      return dataVal;
    }
    DEBUG_LOG("Cache expired");
  } else {
    DEBUG_LOG("Cache error: {}", data.error());
  }

  fn handleApiResult = [](const Result<WeatherOutput>& result) -> WeatherOutput {
    if (!result)
      ERROR_LOG("API request failed: {}", result.error());

    // Fix for second warning: Check the write result
    if (Result<void> writeResult = WriteCacheToFile(*result); !writeResult)
      ERROR_LOG("Failed to write cache: {}", writeResult.error());

    return *result;
  };

  if (std::holds_alternative<std::string>(location)) {
    const auto& city    = std::get<std::string>(location);
    char*       escaped = curl_easy_escape(nullptr, city.c_str(), static_cast<int>(city.length()));

    DEBUG_LOG("Requesting city: {}", escaped);
    const std::string apiUrl =
      fmt::format("https://api.openweathermap.org/data/2.5/weather?q={}&appid={}&units={}", escaped, api_key, units);

    curl_free(escaped);
    return handleApiResult(MakeApiRequest(apiUrl));
  }

  const auto& [lat, lon] = std::get<Coords>(location);
  DEBUG_LOG("Requesting coordinates: lat={:.3f}, lon={:.3f}", lat, lon);

  const std::string apiUrl = fmt::format(
    "https://api.openweathermap.org/data/2.5/weather?lat={:.3f}&lon={:.3f}&appid={}&units={}", lat, lon, api_key, units
  );

  return handleApiResult(MakeApiRequest(apiUrl));
}
