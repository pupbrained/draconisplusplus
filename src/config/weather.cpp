#include <rfl.hpp>
#include <rfl/toml.hpp>
#include <toml++/toml.h>
#include <unistd.h>

#include "config.h"

using namespace std;
using namespace chrono;
using namespace boost;

// Function to read cache from file
optional<pair<json::object, system_clock::time_point>> ReadCacheFromFile() {
  const string cacheFile = "/tmp/weather_cache.json";
  ifstream ifs(cacheFile);

  if (!ifs.is_open()) {
    fmt::println("Cache file not found.");
    return nullopt;
  }

  fmt::println("Reading from cache file...");

  json::object cachedData;
  system_clock::time_point timestamp;

  try {
    json::value val;
    ifs >> val;
    cachedData = val.as_object();

    string tsStr = cachedData["timestamp"].as_string().c_str();
    timestamp    = system_clock::time_point(milliseconds(stoll(tsStr)));

    cachedData.erase("timestamp");
  } catch (...) {
    fmt::println(stderr, "Failed to read from cache file.");

    return nullopt;
  }

  fmt::println("Successfully read from cache file.");
  return make_pair(cachedData, timestamp);
}

// Function to write cache to file
void WriteCacheToFile(const json::object& data) {
  const string cacheFile = "/tmp/weather_cache.json";
  fmt::println("Writing to cache file...");
  ofstream ofs(cacheFile);

  if (!ofs.is_open()) {
    fmt::println(stderr, "Failed to open cache file for writing.");
    return;
  }

  json::object dataToWrite = data;

  dataToWrite["timestamp"] = to_string(
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count());

  ofs << json::serialize(dataToWrite);

  fmt::println("Successfully wrote to cache file.");
}

// Function to make API request
json::object MakeApiRequest(const string& url) {
  using namespace cpr;

  fmt::println("Making API request...");
  const Response res = Get(Url {url});
  fmt::println("Received response from API.");
  json::value json = json::parse(res.text);
  return json.as_object();
}

// Core function to get weather information
json::object Weather::getWeatherInfo() const {
  using namespace cpr;

  const Location loc  = m_Location;
  const string apiKey = m_ApiKey;
  const string units  = m_Units;

  // Check if cache is valid
  if (auto cachedData = ReadCacheFromFile()) {
    auto [data, timestamp] = *cachedData;

    if (system_clock::now() - timestamp <
        minutes(10)) { // Assuming cache duration is always 10 minutes
      fmt::println("Cache is valid. Returning cached data.");
      return data;
    }

    fmt::println("Cache is expired.");
  } else {
    fmt::println("No valid cache found.");
  }

  json::object result;

  if (holds_alternative<string>(loc)) {
    const string city = get<string>(loc);

    const char* location = curl_easy_escape(nullptr, city.c_str(),
                                            static_cast<int>(city.length()));

    fmt::println("City: {}", location);

    const string apiUrl = format(
        "https://api.openweathermap.org/data/2.5/"
        "weather?q={}&appid={}&units={}",
        location, apiKey, units);

    result = MakeApiRequest(apiUrl);
  } else {
    const auto [lat, lon] = get<Coords>(loc);

    fmt::println("Coordinates: lat = {:.3f}, lon = {:.3f}", lat, lon);

    const string apiUrl = format(
        "https://api.openweathermap.org/data/2.5/"
        "weather?lat={:.3f}&lon={:.3f}&appid={}&units={}",
        lat, lon, apiKey, units);

    result = MakeApiRequest(apiUrl);
  }

  // Update the cache with the new data
  WriteCacheToFile(result);

  fmt::println("Returning new data.");

  return result;
}
