#include <chrono>
#include <curl/curl.h>
#include <filesystem>
#include <fmt/core.h>
#include <rfl/json.hpp>
#include <rfl/json/load.hpp>

#include "config.h"

using rfl::Error;
using rfl::Result;
namespace fs = std::filesystem;

namespace {
  // Common function to get cache path
  fn GetCachePath() -> Result<fs::path> {
    std::error_code errc;
    fs::path        cachePath = fs::temp_directory_path(errc);

    if (errc)
      return Error("Failed to get temp directory: " + errc.message());

    cachePath /= "weather_cache.json";
    return cachePath;
  }

  // Function to read cache from file
  fn ReadCacheFromFile() -> Result<WeatherOutput> {
    Result<fs::path> cachePath = GetCachePath();
    if (!cachePath)
      return Error(cachePath.error()->what());

    std::ifstream ifs(*cachePath, std::ios::binary);
    if (!ifs.is_open())
      return Error("Cache file not found: " + cachePath.value().string());

    DEBUG_LOG("Reading from cache file...");

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    Result<WeatherOutput> result = rfl::json::read<WeatherOutput>(content);

    DEBUG_LOG("Successfully read from cache file.");
    return result;
  }

  // Function to write cache to file
  fn WriteCacheToFile(const WeatherOutput& data) -> Result<u8> {
    Result<fs::path> cachePath = GetCachePath();
    if (!cachePath)
      return Error(cachePath.error()->what());

    DEBUG_LOG("Writing to cache file...");

    // Write to temporary file first
    fs::path tempPath = *cachePath;
    tempPath += ".tmp";

    {
      std::ofstream ofs(tempPath, std::ios::binary | std::ios::trunc);
      if (!ofs.is_open())
        return Error("Failed to open temp file: " + tempPath.string());

      auto json = rfl::json::write(data);
      ofs << json;

      if (!ofs)
        return Error("Failed to write to temp file");
    } // File stream closes here

    // Atomic replace
    std::error_code errc;
    fs::rename(tempPath, *cachePath, errc);

    if (errc) {
      fs::remove(tempPath, errc);
      return Error("Failed to replace cache file: " + errc.message());
    }

    DEBUG_LOG("Successfully wrote to cache file.");
    return 0;
  }

  fn WriteCallback(void* contents, const usize size, const usize nmemb, string* str) -> usize {
    const usize totalSize = size * nmemb;
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
  }

  // Function to make API request
  fn MakeApiRequest(const string& url) -> Result<WeatherOutput> {
    DEBUG_LOG("Making API request to URL: {}", url);

    CURL*  curl = curl_easy_init();
    string responseBuffer;

    if (curl) {
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
      const CURLcode res = curl_easy_perform(curl);
      curl_easy_cleanup(curl);

      if (res != CURLE_OK)
        return Error(fmt::format("Failed to perform cURL request: {}", curl_easy_strerror(res)));

      DEBUG_LOG("Received response from API. Response size: {}", responseBuffer.size());
      DEBUG_LOG("Response: {}", responseBuffer);

      WeatherOutput output = rfl::json::read<WeatherOutput>(responseBuffer).value();

      return output; // Return an empty result for now
    }

    return Error("Failed to initialize cURL.");
  }
}

// Core function to get weather information
fn Weather::getWeatherInfo() const -> WeatherOutput {
  using namespace std::chrono;

  // Check if cache is valid
  if (Result<WeatherOutput> data = ReadCacheFromFile()) {
    WeatherOutput dataVal = *data;

    if (system_clock::now() - system_clock::time_point(seconds(dataVal.dt)) < minutes(10)) {
      DEBUG_LOG("Cache is valid. Returning cached data.");

      return dataVal;
    }

    DEBUG_LOG("Cache is expired.");
  } else {
    DEBUG_LOG("No valid cache found.");
  }

  WeatherOutput result;

  if (holds_alternative<string>(location)) {
    const string city = get<string>(location);

    const char* loc = curl_easy_escape(nullptr, city.c_str(), static_cast<int>(city.length()));

    DEBUG_LOG("City: {}", loc);

    const string apiUrl = fmt::format(
      "https://api.openweathermap.org/data/2.5/"
      "weather?q={}&appid={}&units={}",
      loc,
      api_key,
      units
    );

    result = MakeApiRequest(apiUrl).value();
  } else {
    const auto [lat, lon] = get<Coords>(location);

    DEBUG_LOG("Coordinates: lat = {:.3f}, lon = {:.3f}", lat, lon);

    const string apiUrl = fmt::format(
      "https://api.openweathermap.org/data/2.5/"
      "weather?lat={:.3f}&lon={:.3f}&appid={}&units={}",
      lat,
      lon,
      api_key,
      units
    );

    result = MakeApiRequest(apiUrl).value();
  }

  // Update the cache with the new data
  WriteCacheToFile(result);

  DEBUG_LOG("Returning new data.");

  return result;
}
