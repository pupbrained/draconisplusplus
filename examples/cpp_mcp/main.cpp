/**
 * @file main.cpp
 * @brief Draconis++ MCP server example
 *
 * This example demonstrates how to create an MCP server that exposes
 * Draconis++ library functionality via standard input/output, making it
 * compatible with stdio-based MCP clients.
 */

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <magic_enum/magic_enum.hpp>
#include <matchit.hpp>
#include <mcp_message.h>
#include <mcp_tool.h>

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>
#include <Drac++/Services/Weather.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/DataTypes.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::utils::types;
using namespace draconis::core::system;
using namespace draconis::services::weather;
using namespace draconis::services::packages;
using enum draconis::utils::error::DracErrorCode;
using enum draconis::utils::logging::LogLevel;

namespace {
  fn GetCacheManager() -> draconis::utils::cache::CacheManager& {
    static draconis::utils::cache::CacheManager CacheManager;
    return CacheManager;
  }

#define DRAC_SET_INFO(json_obj, key, result_expr, success_handler) \
  {                                                                \
    Result res = (result_expr);                                    \
    if (res)                                                       \
      (json_obj)[key] = (success_handler)(*res);                   \
    else                                                           \
      (json_obj)[key] = {                                          \
        { "error", res.error().message }                           \
      };                                                           \
  }

#define DRAC_RETURN_TEXT(text_content) \
  return {                             \
    {                                  \
     { "type", "text" },              \
     { "text", text_content },        \
     },                                 \
  };

  fn SystemInfoHandler(const mcp::json& /*params*/, const String& /*session_id*/) -> mcp::json {
    mcp::json info = mcp::json::object();

    DRAC_SET_INFO(
      info,
      "os",
      GetOperatingSystem(GetCacheManager()),
      ([](const OSInfo& osInfo) {
        return mcp::json {
          {    "name",    osInfo.name },
          { "version", osInfo.version },
        };
      })
    );

    DRAC_SET_INFO(
      info,
      "kernel",
      GetKernelVersion(GetCacheManager()),
      [](const String& kernelVersion) { return kernelVersion; }
    );

    DRAC_SET_INFO(
      info,
      "host",
      GetHost(GetCacheManager()),
      [](const String& hostInfo) { return hostInfo; }
    );

    DRAC_SET_INFO(
      info,
      "shell",
      GetShell(GetCacheManager()),
      [](const String& shellInfo) { return shellInfo; }
    );

    DRAC_SET_INFO(
      info,
      "desktop_environment",
      GetDesktopEnvironment(GetCacheManager()),
      [](const String& desktopEnvironmentInfo) { return desktopEnvironmentInfo; }
    );

    DRAC_SET_INFO(
      info,
      "window_manager",
      GetWindowManager(GetCacheManager()),
      [](const String& windowManagerInfo) { return windowManagerInfo; }
    );

    DRAC_SET_INFO(
      info,
      "cpu_model",
      GetCPUModel(GetCacheManager()),
      [](const String& cpuModel) { return cpuModel; }
    );

    DRAC_SET_INFO(
      info,
      "cpu_cores",
      GetCPUCores(GetCacheManager()),
      ([](const CPUCores& cpuCores) {
        return mcp::json {
          { "physical", cpuCores.physical },
          {  "logical",  cpuCores.logical }
        };
      })
    );

    DRAC_RETURN_TEXT(info.dump(2))
  }

  fn HardwareInfoHandler(const mcp::json& /*params*/, const String& /*session_id*/) -> mcp::json {
    mcp::json info = mcp::json::object();

    DRAC_SET_INFO(
      info,
      "cpu_model",
      GetCPUModel(GetCacheManager()),
      [](const String& cpuModel) { return cpuModel; }
    );

    DRAC_SET_INFO(
      info,
      "cpu_cores",
      GetCPUCores(GetCacheManager()),
      ([](const CPUCores& cpuCores) {
        return mcp::json {
          { "physical", cpuCores.physical },
          {  "logical",  cpuCores.logical }
        };
      })
    );

    DRAC_SET_INFO(
      info,
      "gpu_model",
      GetGPUModel(GetCacheManager()),
      [](const String& gpuModel) { return gpuModel; }
    );

    DRAC_SET_INFO(
      info,
      "memory",
      GetMemInfo(GetCacheManager()),
      ([](const ResourceUsage& memInfo) {
        return mcp::json {
          {  "used_bytes",  memInfo.usedBytes },
          { "total_bytes", memInfo.totalBytes },
        };
      })
    );

    DRAC_SET_INFO(
      info,
      "disk",
      GetDiskUsage(GetCacheManager()),
      ([](const ResourceUsage& diskInfo) {
        return mcp::json {
          {  "used_bytes",  diskInfo.usedBytes },
          { "total_bytes", diskInfo.totalBytes },
        };
      })
    );

    DRAC_SET_INFO(
      info,
      "battery",
      GetBatteryInfo(GetCacheManager()),
      ([](const Battery& batteryInfo) {
        return mcp::json {
          {     "percentage",                                 batteryInfo.percentage.value_or(-1) },
          {         "status",                           magic_enum::enum_name(batteryInfo.status) },
          { "time_remaining", batteryInfo.timeRemaining.value_or(std::chrono::seconds(0)).count() },
        };
      })
    );

    DRAC_RETURN_TEXT(info.dump(2))
  }

#undef DRAC_SET_INFO

  fn WeatherHandler(const mcp::json& params, const String& /*session_id*/) -> mcp::json {
#if DRAC_ENABLE_WEATHER
    Result<Coords> coordsResult;
    String         location;

    if (params.contains("location") && !params["location"].empty() && params["location"] != "" && params["location"] != "null") {
      location = params["location"];

      coordsResult = Geocode(location);

      if (!coordsResult)
        DRAC_RETURN_TEXT("Failed to geocode location '" + location + "': " + coordsResult.error().message);
    } else {
      Result<IPLocationInfo> locationInfoResult = GetCurrentLocationInfoFromIP();
      if (!locationInfoResult)
        DRAC_RETURN_TEXT("Failed to get current location from IP: " + locationInfoResult.error().message);

      const IPLocationInfo& locationInfo = *locationInfoResult;
      coordsResult                       = locationInfo.coords;
      location                           = locationInfo.locationName;
    }

    UniquePointer<IWeatherService> weatherService = CreateWeatherService(
      Provider::MetNo,
      *coordsResult,
      UnitSystem::Imperial
    );

    if (!weatherService)
      DRAC_RETURN_TEXT("Failed to create weather service")

    String weatherCacheKey = "weather_" + location + "_" + std::to_string(coordsResult->lat) + "_" + std::to_string(coordsResult->lon);

    Result<Report> weatherResult = GetCacheManager().getOrSet<Report>(
      weatherCacheKey,
      [&]() -> Result<Report> {
        return weatherService->getWeatherInfo();
      }
    );

    if (!weatherResult)
      DRAC_RETURN_TEXT("Failed to fetch weather data: " + weatherResult.error().message);

    const Report& report      = *weatherResult;
    mcp::json     weatherInfo = {
      { "temperature", report.temperature },
      { "description", report.description },
      {    "location",           location },
    };

    if (report.name.has_value())
      weatherInfo["resolved_location"] = *report.name;

    DRAC_RETURN_TEXT(weatherInfo.dump(2))
#else
    DRAC_RETURN_TEXT("Weather service not enabled in this build")
#endif
  }

  fn PackageCountHandler(const mcp::json& params, const String& /*session_id*/) -> mcp::json {
#if DRAC_ENABLE_PACKAGECOUNT
    using enum Manager;

    Manager enabledManagers = NONE;

    static const UnorderedMap<String, Manager> MANAGER_MAP = {
      {  "cargo",  CARGO },
  #if defined(__linux__) || defined(__APPLE__)
      {    "nix",    NIX },
  #endif
  #ifdef __linux__
      {    "apk",    APK },
      {   "dpkg",   DPKG },
      {   "moss",   MOSS },
      { "pacman", PACMAN },
      {    "rpm",    RPM },
      {   "xbps",   XBPS },
  #elifdef __APPLE__
      { "homebrew", HOMEBREW },
      { "macports", MACPORTS },
  #elifdef _WIN32
      { "winget", WINGET },
      { "chocolatey", CHOCOLATEY },
      { "scoop", SCOOP },
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
      { "pkgng", PKGNG },
  #elifdef __NetBSD__
      { "pkgsrc", PKGSRC },
  #elifdef __HAIKU__
      { "haikupkg", HAIKUPKG },
  #endif
    };

    if (params.contains("managers") && !params["managers"].empty()) {
      String      managersStr = params["managers"];
      Vec<String> managersList;
      usize       pos = 0;

      while ((pos = managersStr.find(',')) != String::npos) {
        String token = managersStr.substr(0, pos);
        managersList.push_back(token);
        managersStr.erase(0, pos + 1);
      }

      managersList.push_back(managersStr);

      for (const String& mgr : managersList) {
        auto iter = MANAGER_MAP.find(mgr);

        if (iter != MANAGER_MAP.end())
          enabledManagers |= iter->second;
        else
          warn_log("Invalid package manager: {}", mgr);
      }
    } else
      for (const auto& pair : MANAGER_MAP)
        enabledManagers |= pair.second;

    if (enabledManagers == NONE)
      DRAC_RETURN_TEXT("No valid package managers specified or available")

    Result<Map<String, u64>> countResult = GetIndividualCounts(GetCacheManager(), enabledManagers);

    if (countResult) {
      mcp::json packageInfo = mcp::json::object();
      u64       totalCount  = 0;

      for (const auto& [manager, count] : *countResult) {
        packageInfo[manager] = count;
        totalCount += count;
      }

      packageInfo["total"] = totalCount;

      DRAC_RETURN_TEXT(packageInfo.dump(2))
    }

    DRAC_RETURN_TEXT("Failed to get package count: " + countResult.error().message)
#else
    DRAC_RETURN_TEXT("Package counting not enabled in this build")
#endif
  }

  fn NetworkInfoHandler(const mcp::json& /*params*/, const String& /*session_id*/) -> mcp::json {
    mcp::json info = mcp::json::object();

    if (Result interfacesResult = GetNetworkInterfaces(GetCacheManager()); interfacesResult) {
      mcp::json interfaces = mcp::json::array();
      for (const NetworkInterface& iface : *interfacesResult) {
        mcp::json interfaceInfo = {
          {        "name",       iface.name },
          {       "is_up",       iface.isUp },
          { "is_loopback", iface.isLoopback }
        };

        if (iface.ipv4Address.has_value())
          interfaceInfo["ipv4_address"] = *iface.ipv4Address;
        if (iface.ipv6Address.has_value())
          interfaceInfo["ipv6_address"] = *iface.ipv6Address;
        if (iface.macAddress.has_value())
          interfaceInfo["mac_address"] = *iface.macAddress;

        interfaces.push_back(interfaceInfo);
      }
      info["interfaces"] = interfaces;
    } else
      info["interfaces"] = {
        { "error", interfacesResult.error().message },
      };

    if (Result primaryResult = GetPrimaryNetworkInterface(GetCacheManager()); primaryResult) {
      mcp::json primaryInfo = {
        {        "name",       primaryResult->name },
        {       "is_up",       primaryResult->isUp },
        { "is_loopback", primaryResult->isLoopback },
      };

      if (primaryResult->ipv4Address.has_value())
        primaryInfo["ipv4_address"] = *primaryResult->ipv4Address;
      if (primaryResult->ipv6Address.has_value())
        primaryInfo["ipv6_address"] = *primaryResult->ipv6Address;
      if (primaryResult->macAddress.has_value())
        primaryInfo["mac_address"] = *primaryResult->macAddress;

      info["primary_interface"] = primaryInfo;
    } else
      info["primary_interface"] = {
        { "error", primaryResult.error().message },
      };

    DRAC_RETURN_TEXT(info.dump(2))
  }

  fn DisplayInfoHandler(const mcp::json& /*params*/, const String& /*session_id*/) -> mcp::json {
    mcp::json info = mcp::json::object();

    if (Result displaysResult = GetOutputs(GetCacheManager()); displaysResult) {
      mcp::json displays = mcp::json::array();

      for (const DisplayInfo& display : *displaysResult)
        displays.push_back({
          {           "id",                display.id },
          {        "width",  display.resolution.width },
          {       "height", display.resolution.height },
          { "refresh_rate",       display.refreshRate },
          {   "is_primary",         display.isPrimary }
        });

      info["displays"] = displays;
    } else
      info["displays"] = {
        { "error", displaysResult.error().message },
      };

    if (Result primaryResult = GetPrimaryOutput(GetCacheManager()); primaryResult)
      info["primary_display"] = {
        {           "id",                primaryResult->id },
        {        "width",  primaryResult->resolution.width },
        {       "height", primaryResult->resolution.height },
        { "refresh_rate",       primaryResult->refreshRate },
      };
    else
      info["primary_display"] = {
        { "error", primaryResult.error().message },
      };

    DRAC_RETURN_TEXT(info.dump(2))
  }

  fn UptimeHandler(const mcp::json& /*params*/, const String& /*session_id*/) -> mcp::json {
    Result uptimeResult = GetUptime();

    if (uptimeResult) {
      u32 seconds          = uptimeResult->count();
      u32 hours            = seconds / 3600;
      u32 minutes          = (seconds % 3600) / 60;
      u32 remainingSeconds = seconds % 60;

      String uptimeStr = std::format("{}h {}m {}s", hours, minutes, remainingSeconds);

      DRAC_RETURN_TEXT("System uptime: " + uptimeStr)
    }

    DRAC_RETURN_TEXT("Failed to get uptime: " + uptimeResult.error().message)
  }

  fn NowPlayingHandler(const mcp::json& /*params*/, const String& /*session_id*/) -> mcp::json {
#if DRAC_ENABLE_NOWPLAYING
    Result nowPlayingResult = GetNowPlaying();

    if (nowPlayingResult) {
      const MediaInfo& mediaInfo = *nowPlayingResult;

      mcp::json nowPlayingInfo = mcp::json::object();

      if (mediaInfo.title.has_value())
        nowPlayingInfo["title"] = *mediaInfo.title;
      else
        nowPlayingInfo["title"] = nullptr;

      if (mediaInfo.artist.has_value())
        nowPlayingInfo["artist"] = *mediaInfo.artist;
      else
        nowPlayingInfo["artist"] = nullptr;

      DRAC_RETURN_TEXT(nowPlayingInfo.dump(2))
    }

    DRAC_RETURN_TEXT("Failed to get now playing info: " + nowPlayingResult.error().message)
#else
    DRAC_RETURN_TEXT("Now playing functionality not enabled in this build")
#endif
  }

#undef DRAC_RETURN_TEXT

  template <typename T, typename SuccessHandler>
  fn setJSONFromResult(mcp::json& obj, const String& key, const Result<T>& result, SuccessHandler&& successHandler) -> void {
    if (result)
      obj[key] = std::forward<SuccessHandler>(successHandler)(*result);
    else
      obj[key] = {
        { "error", result.error().message }
      };
  }

  fn ComprehensiveInfoHandler(const mcp::json& params, const String& /*session_id*/) -> mcp::json {
    mcp::json comprehensiveInfo = mcp::json::object();

    CacheManager& cacheManager = GetCacheManager();

    mcp::json systemInfo = mcp::json::object();
    setJSONFromResult(systemInfo, "os", GetOperatingSystem(cacheManager), [](const OSInfo& osInfo) {
      return mcp::json {
        {    "name",    osInfo.name },
        { "version", osInfo.version },
      };
    });
    setJSONFromResult(systemInfo, "kernel", GetKernelVersion(cacheManager), [](const String& kernelVersion) { return kernelVersion; });
    setJSONFromResult(systemInfo, "host", GetHost(cacheManager), [](const String& hostInfo) { return hostInfo; });
    setJSONFromResult(systemInfo, "shell", GetShell(cacheManager), [](const String& shellInfo) { return shellInfo; });
    setJSONFromResult(systemInfo, "desktop_environment", GetDesktopEnvironment(cacheManager), [](const String& desktopEnvironmentInfo) { return desktopEnvironmentInfo; });
    setJSONFromResult(systemInfo, "window_manager", GetWindowManager(cacheManager), [](const String& windowManagerInfo) { return windowManagerInfo; });
    comprehensiveInfo["system"] = systemInfo;

    mcp::json hardwareInfo = mcp::json::object();
    setJSONFromResult(hardwareInfo, "cpu_model", GetCPUModel(cacheManager), [](const String& cpuModel) { return cpuModel; });
    setJSONFromResult(hardwareInfo, "cpu_cores", GetCPUCores(cacheManager), [](const CPUCores& cpuCoresResult) {
      return mcp::json {
        { "physical", cpuCoresResult.physical },
        {  "logical",  cpuCoresResult.logical },
      };
    });
    setJSONFromResult(hardwareInfo, "gpu_model", GetGPUModel(cacheManager), [](const String& gpuModel) { return gpuModel; });
    setJSONFromResult(hardwareInfo, "memory", GetMemInfo(cacheManager), [](const ResourceUsage& memResult) {
      return mcp::json {
        {  "used_bytes",                                                 memResult.usedBytes },
        { "total_bytes",                                                memResult.totalBytes },
        {     "used_gb",  static_cast<f64>(memResult.usedBytes) / (1024.0 * 1024.0 * 1024.0) },
        {    "total_gb", static_cast<f64>(memResult.totalBytes) / (1024.0 * 1024.0 * 1024.0) },
      };
    });
    setJSONFromResult(hardwareInfo, "disk", GetDiskUsage(cacheManager), [](const ResourceUsage& diskResult) {
      return mcp::json {
        {  "used_bytes",                                                 diskResult.usedBytes },
        { "total_bytes",                                                diskResult.totalBytes },
        {     "used_gb",  static_cast<f64>(diskResult.usedBytes) / (1024.0 * 1024.0 * 1024.0) },
        {    "total_gb", static_cast<f64>(diskResult.totalBytes) / (1024.0 * 1024.0 * 1024.0) },
      };
    });
    setJSONFromResult(hardwareInfo, "battery", GetBatteryInfo(cacheManager), [](const Battery& batteryResult) {
      return mcp::json {
        { "percentage",  batteryResult.percentage.value_or(-1) },
        {     "status", static_cast<i32>(batteryResult.status) },
      };
    });
    comprehensiveInfo["hardware"] = hardwareInfo;

    mcp::json networkInfo = mcp::json::object();
    setJSONFromResult(networkInfo, "interfaces", GetNetworkInterfaces(cacheManager), [](const Vec<NetworkInterface>& interfacesResult) {
      mcp::json interfaces = mcp::json::array();
      for (const NetworkInterface& iface : interfacesResult) {
        mcp::json interfaceInfo = {
          {        "name",       iface.name },
          {       "is_up",       iface.isUp },
          { "is_loopback", iface.isLoopback },
        };

        if (iface.ipv4Address.has_value())
          interfaceInfo["ipv4_address"] = *iface.ipv4Address;
        if (iface.ipv6Address.has_value())
          interfaceInfo["ipv6_address"] = *iface.ipv6Address;
        if (iface.macAddress.has_value())
          interfaceInfo["mac_address"] = *iface.macAddress;

        interfaces.push_back(interfaceInfo);
      }
      return interfaces;
    });

    setJSONFromResult(networkInfo, "primary_interface", GetPrimaryNetworkInterface(cacheManager), [](const NetworkInterface& primaryResult) {
      mcp::json primaryInfo = {
        {        "name",       primaryResult.name },
        {       "is_up",       primaryResult.isUp },
        { "is_loopback", primaryResult.isLoopback },
      };

      if (primaryResult.ipv4Address.has_value())
        primaryInfo["ipv4_address"] = *primaryResult.ipv4Address;
      if (primaryResult.ipv6Address.has_value())
        primaryInfo["ipv6_address"] = *primaryResult.ipv6Address;
      if (primaryResult.macAddress.has_value())
        primaryInfo["mac_address"] = *primaryResult.macAddress;

      return primaryInfo;
    });
    comprehensiveInfo["network"] = networkInfo;

    mcp::json displayInfo = mcp::json::object();
    setJSONFromResult(displayInfo, "displays", GetOutputs(cacheManager), [](const Vec<DisplayInfo>& displaysResult) {
      mcp::json displays = mcp::json::array();
      for (const DisplayInfo& display : displaysResult) {
        displays.push_back({
          {           "id",                display.id },
          {        "width",  display.resolution.width },
          {       "height", display.resolution.height },
          { "refresh_rate",       display.refreshRate },
          {   "is_primary",         display.isPrimary },
        });
      }
      return displays;
    });

    setJSONFromResult(displayInfo, "primary_display", GetPrimaryOutput(cacheManager), [](const DisplayInfo& primaryResult) {
      return mcp::json {
        {           "id",                primaryResult.id },
        {        "width",  primaryResult.resolution.width },
        {       "height", primaryResult.resolution.height },
        { "refresh_rate",       primaryResult.refreshRate },
      };
    });
    comprehensiveInfo["display"] = displayInfo;

    setJSONFromResult(comprehensiveInfo, "uptime", GetUptime(), [](const std::chrono::seconds& uptimeResult) {
      u32 seconds          = uptimeResult.count();
      u32 hours            = seconds / 3600;
      u32 minutes          = (seconds % 3600) / 60;
      u32 remainingSeconds = seconds % 60;
      return mcp::json {
        { "seconds", seconds },
        { "formatted", std::format("{}h {}m {}s", hours, minutes, remainingSeconds) },
      };
    });

#if DRAC_ENABLE_WEATHER
    Result<Coords> coordsResult;
    String         location;

    if (params.contains("location") && !params["location"].empty() && params["location"] != "" && params["location"] != "null") {
      location = params["location"];

      coordsResult = Geocode(location);

      if (!coordsResult)
        comprehensiveInfo["weather"] = {
          { "error", "Failed to geocode location '" + location + "': " + coordsResult.error().message },
        };
      else
        comprehensiveInfo["weather"] = {
          { "error", "Failed to initialize geocoding service" },
        };
    } else {
      // Use current location from IP with detailed info
      Result<IPLocationInfo> locationInfoResult = GetCurrentLocationInfoFromIP();
      if (!locationInfoResult) {
        comprehensiveInfo["weather"] = {
          { "error", "Failed to get current location from IP: " + locationInfoResult.error().message },
        };
      } else {
        const IPLocationInfo& locationInfo = *locationInfoResult;
        coordsResult                       = locationInfo.coords;
        location                           = locationInfo.locationName;
      }
    }

    if (coordsResult) {
      UniquePointer<IWeatherService> weatherService = CreateWeatherService(
        Provider::MetNo,
        *coordsResult,
        UnitSystem::Imperial
      );

      if (weatherService) {
        String weatherCacheKey = "weather_" + location + "_" + std::to_string(coordsResult->lat) + "_" + std::to_string(coordsResult->lon);
        setJSONFromResult(comprehensiveInfo, "weather", cacheManager.getOrSet<Report>(weatherCacheKey, [&]() -> Result<Report> { return weatherService->getWeatherInfo(); }), [&](const Report& report) {
          mcp::json weatherInfo = {
            { "temperature", report.temperature },
            { "description", report.description },
            {    "location",           location },
          };

          if (report.name.has_value())
            weatherInfo["resolved_location"] = *report.name;

          return weatherInfo;
        });
      } else {
        comprehensiveInfo["weather"] = {
          { "error", "Failed to create weather service" },
        };
      }
    }
#else
    comprehensiveInfo["weather"] = {
      { "error", "Weather service not enabled in this build" },
    };
#endif

#if DRAC_ENABLE_PACKAGECOUNT
    Manager enabledManagers = Manager::CARGO;
  #if defined(__linux__) || defined(__APPLE__)
    enabledManagers |= Manager::NIX;
  #endif
  #ifdef __linux__
    enabledManagers |= Manager::APK | Manager::DPKG | Manager::MOSS |
      Manager::PACMAN | Manager::RPM | Manager::XBPS;
  #elifdef __APPLE__
    enabledManagers |= Manager::HOMEBREW | Manager::MACPORTS;
  #elifdef _WIN32
    enabledManagers |= Manager::WINGET | Manager::CHOCOLATEY | Manager::SCOOP;
  #elif defined(__FreeBSD__) || defined(__DragonFly__)
    enabledManagers |= Manager::PKGNG;
  #elifdef __NetBSD__
    enabledManagers |= Manager::PKGSRC;
  #elifdef __HAIKU__
    enabledManagers |= Manager::HAIKUPKG;
  #endif

    setJSONFromResult(comprehensiveInfo, "packages", GetIndividualCounts(cacheManager, enabledManagers), [](const Map<String, u64>& countResult) {
      mcp::json packageInfo = mcp::json::object();
      u64       totalCount  = 0;

      for (const auto& [manager, count] : countResult) {
        packageInfo[manager] = count;
        totalCount += count;
      }

      packageInfo["total"] = totalCount;
      return packageInfo;
    });
#else
    comprehensiveInfo["packages"] = {
      { "error", "Package counting not enabled in this build" },
    };
#endif

#if DRAC_ENABLE_NOWPLAYING
    setJSONFromResult(comprehensiveInfo, "now_playing", GetNowPlaying(), [](const MediaInfo& mediaInfo) {
      mcp::json nowPlayingInfo = mcp::json::object();

      if (mediaInfo.title.has_value())
        nowPlayingInfo["title"] = *mediaInfo.title;
      else
        nowPlayingInfo["title"] = nullptr;

      if (mediaInfo.artist.has_value())
        nowPlayingInfo["artist"] = *mediaInfo.artist;
      else
        nowPlayingInfo["artist"] = nullptr;
      return nowPlayingInfo;
    });
#else
    comprehensiveInfo["now_playing"] = {
      { "error", "Now playing functionality not enabled in this build" },
    };
#endif

    return {
      {
       { "type", "text" },
       { "text", comprehensiveInfo.dump(2) },
       },
    };
  }
} // namespace

class StdIOServer {
 public:
  StdIOServer(String name, String version)
    : m_name(std::move(name)), m_version(std::move(version)) {}

  fn setCapabilities(const mcp::json& capabilities) -> void {
    m_capabilities = capabilities;
  }

  fn registerTool(const mcp::tool& tool, const Fn<mcp::json(const mcp::json&, const String&)>& handler) -> void {
    m_tools[tool.name] = { tool, handler };
  }

  fn run() -> void {
    String line;

    while (std::getline(std::cin, line)) {
      mcp::request req;
      bool         isNotification = false;

      try {
        mcp::json requestJson = mcp::json::parse(line);

        req.jsonrpc = requestJson.value("jsonrpc", "2.0");
        req.id      = requestJson.contains("id") ? requestJson["id"] : nullptr;
        req.method  = requestJson.value("method", "");
        req.params  = requestJson.value("params", mcp::json::object());

        isNotification = req.id.is_null();

        mcp::json result = processRequest(req).value_or(mcp::json::object());

        if (!isNotification) {
          mcp::json responseJson = {
            { "jsonrpc",  "2.0" },
            {      "id", req.id },
            {  "result", result },
          };

          std::cout << responseJson.dump() << '\n';
          std::cout.flush();
        }
      } catch (const Exception& e) {
        if (!isNotification && !req.id.is_null()) {
          mcp::json errorResponse = {
            { "jsonrpc", "2.0" },
            { "id", req.id },
            {
             "error",
             {
                { "code", -32603 },
                { "message", "Internal error: " + String(e.what()) },
              },
             },
          };

          std::cout << errorResponse.dump() << '\n';
          std::cout.flush();
        }
      }
    }
  }

 private:
  using ToolHandler = Fn<mcp::json(const mcp::json&, const String&)>;
  using Tools       = Map<String, Pair<mcp::tool, ToolHandler>>;

  String    m_name;
  String    m_version;
  mcp::json m_capabilities;
  Tools     m_tools;

  fn processRequest(const mcp::request& req) -> Result<mcp::json> {
    using draconis::utils::error::DracError;
    using matchit::match, matchit::is, matchit::or_, matchit::_;

    return match(req.method)(
      is | "initialize"                             = handleInitialize(req),
      is | "tools/list"                             = handleToolsList(req),
      is | "tools/call"                             = handleToolsCall(req),
      is | "resources/list"                         = handleResourcesList(req),
      is | or_("ping", "notifications/initialized") = mcp::json::object(),
      is | _                                        = Err(DracError(InvalidArgument, std::format("Unknown method: {}", req.method)))
    );
  }

  fn handleInitialize(const mcp::request& /*req*/) -> mcp::json {
    return mcp::json {
      { "protocolVersion", "2024-11-05" },
      { "capabilities", m_capabilities },
      {
       "serverInfo",
       {
          { "name", m_name },
          { "version", m_version },
        },
       },
    };
  }

  fn handleToolsList(const mcp::request& /*req*/) -> mcp::json {
    mcp::json toolsArray = mcp::json::array();

    for (const auto& [_, tool_pair] : m_tools)
      toolsArray.push_back(tool_pair.first.to_json());

    return {
      { "tools", toolsArray },
    };
  }

  fn handleToolsCall(const mcp::request& req) -> Result<mcp::json> {
    if (!req.params.contains("name"))
      ERR_FMT(InvalidArgument, "Missing tool name");

    String toolName = req.params["name"];
    auto   iter     = m_tools.find(toolName);

    if (iter == m_tools.end())
      ERR_FMT(InvalidArgument, "Tool not found: {}", toolName);

    mcp::json arguments = req.params.value("arguments", mcp::json::object());
    String    sessionId = req.params.value("sessionId", "");

    mcp::json result = iter->second.second(arguments, sessionId);

    return mcp::json {
      { "content", result },
    };
  }

  static fn handleResourcesList(const mcp::request& /*req*/) -> mcp::json {
    return {
      { "resources", mcp::json::array() },
    };
  }
};

fn main() -> i32 {
  draconis::utils::logging::SetRuntimeLogLevel(draconis::utils::logging::LogLevel::Debug);

  StdIOServer server("Draconis++ MCP Server", DRAC_VERSION);

  mcp::json capabilities = {
    { "tools", mcp::json::object() },
  };

  server.setCapabilities(capabilities);

  mcp::tool systemInfoTool =
    mcp::tool_builder("system_info")
      .with_description("Get system information (OS, kernel, host, shell, desktop environment, window manager)")
      .build();

  mcp::tool hardwareInfoTool =
    mcp::tool_builder("hardware_info")
      .with_description("Get hardware information (CPU, GPU, memory, disk, battery)")
      .build();

  mcp::tool weatherTool =
    mcp::tool_builder("weather")
      .with_description("Get current weather information. If no location is specified, automatically detects your current location from IP address.")
      .with_string_param("location", "Location name (e.g., 'New York, NY', 'London, UK', 'Tokyo, Japan'). Omit this parameter to use your current location.", false)
      .build();

  mcp::tool packageCountTool =
    mcp::tool_builder("package_count")
      .with_description("Get individual package counts from available package managers")
      .with_string_param("managers", "Comma-separated list of package managers to check (e.g., 'pacman,dpkg,cargo')", false)
      .build();

  mcp::tool networkInfoTool =
    mcp::tool_builder("network_info")
      .with_description("Get network interface information")
      .build();

  mcp::tool displayInfoTool =
    mcp::tool_builder("display_info")
      .with_description("Get display/monitor information")
      .build();

  mcp::tool uptimeTool =
    mcp::tool_builder("uptime")
      .with_description("Get system uptime")
      .build();

  mcp::tool nowPlayingTool =
    mcp::tool_builder("now_playing")
      .with_description("Get currently playing media information (title and artist)")
      .build();

  mcp::tool comprehensiveTool =
    mcp::tool_builder("comprehensive_info")
      .with_description("Get all system information at once (system, hardware, network, display, uptime, weather, individual package counts)")
      .with_string_param("location", "Location name for weather information (e.g., 'New York, NY', 'London, UK'). Omit this parameter to use your current location for weather.", false)
      .build();

  server.registerTool(systemInfoTool, SystemInfoHandler);
  server.registerTool(hardwareInfoTool, HardwareInfoHandler);
  server.registerTool(weatherTool, WeatherHandler);
  server.registerTool(packageCountTool, PackageCountHandler);
  server.registerTool(networkInfoTool, NetworkInfoHandler);
  server.registerTool(displayInfoTool, DisplayInfoHandler);
  server.registerTool(uptimeTool, UptimeHandler);
  server.registerTool(nowPlayingTool, NowPlayingHandler);
  server.registerTool(comprehensiveTool, ComprehensiveInfoHandler);

  server.run();

  return EXIT_SUCCESS;
}
