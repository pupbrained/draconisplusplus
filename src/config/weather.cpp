#include "config.h"

// Function to read cache from file
std::optional<std::pair<co::Json, std::chrono::system_clock::time_point>>
ReadCacheFromFile() {
  const string cacheFile = "/tmp/weather_cache.json";
  std::ifstream ifs(cacheFile);

  if (!ifs.is_open()) {
    fmt::println("Cache file not found.");
    return std::nullopt;
  }

  fmt::println("Reading from cache file...");

  co::Json val;
  std::chrono::system_clock::time_point timestamp;

  try {
    std::stringstream buf;
    buf << ifs.rdbuf();

    val.parse_from(buf.str());

    string tsStr = val["timestamp"].as_string().c_str();
    timestamp    = std::chrono::system_clock::time_point(
        std::chrono::milliseconds(stoll(tsStr)));

    val.erase("timestamp");
  } catch (...) {
    fmt::println(stderr, "Failed to read from cache file.");

    return std::nullopt;
  }

  fmt::println("Successfully read from cache file.");
  return make_pair(val, timestamp);
}

// Function to write cache to file
void WriteCacheToFile(const co::Json& data) {
  const string cacheFile = "/tmp/weather_cache.json";
  fmt::println("Writing to cache file...");
  std::ofstream ofs(cacheFile);

  if (!ofs.is_open()) {
    fmt::println(stderr, "Failed to open cache file for writing.");
    return;
  }

  data["timestamp"] =
      std::to_string(duration_cast<std::chrono::milliseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count());

  ofs << data.as_string();

  fmt::println("Successfully wrote to cache file.");
}

// Function to make API request
co::Json MakeApiRequest(const string& url) {
  using namespace cpr;

  fmt::println("Making API request...");
  const Response res = Get(Url {url});
  fmt::println("Received response from API.");
  co::Json json = json::parse(res.text);

  return json;
}

// Core function to get weather information
co::Json Weather::getWeatherInfo() const {
  using namespace cpr;

  const Location loc  = m_Location;
  const string apiKey = m_ApiKey;
  const string units  = m_Units;

  // Check if cache is valid
  if (auto cachedData = ReadCacheFromFile()) {
    auto& [data, timestamp] = *cachedData;

    if (std::chrono::system_clock::now() - timestamp <
        std::chrono::minutes(
            10)) { // Assuming cache duration is always 10 minutes
      fmt::println("Cache is valid. Returning cached data.");
      return data;
    }

    fmt::println("Cache is expired.");
  } else {
    fmt::println("No valid cache found.");
  }

  co::Json result;

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
