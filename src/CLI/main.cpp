#include <array>
#include <cstdlib> // EXIT_FAILURE, EXIT_SUCCESS

#ifdef _WIN32
  #include <fcntl.h>
  #include <io.h>
  #include <windows.h>
#endif

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>

#if DRAC_ENABLE_WEATHER
  #include <Drac++/Services/Weather.hpp>
#endif

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"
#include "UI/UI.hpp"

using namespace draconis::utils::types;
using namespace draconis::utils::logging;
using namespace draconis::core::system;
using namespace draconis::config;
using namespace draconis::ui;

using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

#if DRAC_ENABLE_WEATHER
using draconis::services::weather::Report;
#endif

namespace {
#ifdef _WIN32
  fn ConvertUTF8ToWString(const String& utf8String) -> Option<WString> {
    const i32 wideSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), static_cast<i32>(utf8String.length()), nullptr, 0);

    if (wideSize <= 0)
      return None;

    WString wideString(wideSize, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), static_cast<i32>(utf8String.length()), wideString.data(), wideSize);

    return wideString;
  }

  fn WriteToConsole(const String& document) -> Unit {
    SetConsoleOutputCP(CP_UTF8);

    if (Option<WString> wideDocument = ConvertUTF8ToWString(document)) {
      HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

      if (hConsole != INVALID_HANDLE_VALUE) {
        DWORD written {};
        WriteConsoleW(hConsole, wideDocument->c_str(), static_cast<DWORD>(wideDocument->length()), &written, nullptr);
        WriteConsoleW(hConsole, L"\n", 1, &written, nullptr);
        return;
      }
    }

    Println(document);
  }
#endif

  fn PrintDoctorReport(
#if DRAC_ENABLE_WEATHER
    const Result<Report>& weather,
#endif
    const SystemInfo& data
  ) -> Unit {
    Array<Option<Pair<String, DracError>>, 10 + DRAC_ENABLE_PACKAGECOUNT + DRAC_ENABLE_NOWPLAYING + DRAC_ENABLE_WEATHER>
      failures {};

    usize failureCount = 0;

#define DRAC_CHECK(expr, label) \
  if (!(expr))                  \
  failures.at(failureCount++) = { label, (expr).error() }

    DRAC_CHECK(data.date, "Date");
    DRAC_CHECK(data.host, "Host");
    DRAC_CHECK(data.kernelVersion, "KernelVersion");
    DRAC_CHECK(data.osVersion, "OSVersion");
    DRAC_CHECK(data.memInfo, "MemoryInfo");
    DRAC_CHECK(data.desktopEnv, "DesktopEnvironment");
    DRAC_CHECK(data.windowMgr, "WindowManager");
    DRAC_CHECK(data.diskUsage, "DiskUsage");
    DRAC_CHECK(data.shell, "Shell");
    DRAC_CHECK(data.uptime, "Uptime");

#if DRAC_ENABLE_PACKAGECOUNT
    DRAC_CHECK(data.packageCount, "PackageCount");
#endif

#if DRAC_ENABLE_NOWPLAYING
    DRAC_CHECK(data.nowPlaying, "NowPlaying");
#endif

#if DRAC_ENABLE_WEATHER
    DRAC_CHECK(weather, "Weather");
#endif

    if (failureCount == 0)
      Println("All readouts were successful!");
    else {
      Println(
        "Out of {} readouts, {} failed.\n",
        failures.size(),
        failureCount
      );

      for (const Option<Pair<String, DracError>>& failure : failures)
        if (failure)
          Println(
            R"(Readout "{}" failed: {} ({}))",
            failure->first,
            failure->second.message,
            magic_enum::enum_name(failure->second.code)
          );
    }
  }
} // namespace

fn main(const i32 argc, char* argv[]) -> i32 try {
#ifdef _WIN32
  winrt::init_apartment();
#endif

  bool doctorMode     = false;
  bool clearCache     = false;
  bool ignoreCacheRun = false;

  {
    using draconis::utils::argparse::ArgumentParser;

    ArgumentParser parser("draconis", DRAC_VERSION);

    parser
      .addArguments("-V", "--verbose")
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    parser
      .addArguments("-d", "--doctor")
      .help("Reports any failed readouts and their error messages.")
      .flag();

    parser
      .addArguments("-l", "--log-level")
      .help("Set the minimum log level.")
      .defaultValue(LogLevel::Info);

    parser
      .addArguments("--clear-cache")
      .help("Clears the cache. This will remove all cached data, including in-memory and on-disk copies.")
      .flag();

    parser
      .addArguments("--ignore-cache")
      .help("Ignore cache for this run (fetch fresh data without reading/writing on-disk cache).")
      .flag();

    if (Result result = parser.parseArgs(Span(argv, static_cast<usize>(argc))); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    doctorMode     = parser.get<bool>("-d") || parser.get<bool>("--doctor");
    clearCache     = parser.get<bool>("--clear-cache");
    ignoreCacheRun = parser.get<bool>("--ignore-cache");

    SetRuntimeLogLevel(
      parser.get<bool>("-V") || parser.get<bool>("--verbose")
        ? LogLevel::Debug
        : parser.getEnum<LogLevel>("--log-level")
    );
  }

  using draconis::utils::cache::CacheManager, draconis::utils::cache::CachePolicy;

  CacheManager cache;

  if (ignoreCacheRun)
    CacheManager::ignoreCache = true;

  cache.setGlobalPolicy(CachePolicy::tempDirectory());

  if (clearCache) {
    const u8 removedCount = cache.invalidateAll(true);

    if (removedCount > 0)
      Println("Removed {} files.", removedCount);
    else
      Println("No cache files were found to clear.");

    return EXIT_SUCCESS;
  }

#ifndef NDEBUG
  if (Result<CPUCores> cpuCores = GetCPUCores(cache))
    debug_log("CPU cores: {} physical, {} logical", cpuCores->physical, cpuCores->logical);
  else
    debug_at(cpuCores.error());

  if (Result<NetworkInterface> networkInterface = GetPrimaryNetworkInterface(cache)) {
    debug_log("Network interface: {}", networkInterface->name);
    debug_log("Network interface IPv4 address: {}", networkInterface->ipv4Address.value_or("N/A"));
    debug_log("Network interface MAC address: {}", networkInterface->macAddress.value_or("N/A"));
    debug_log("Network interface is up: {}", networkInterface->isUp);
    debug_log("Network interface is loopback: {}", networkInterface->isLoopback);
  } else
    debug_at(networkInterface.error());

  if (Result<Battery> battery = GetBatteryInfo(cache)) {
    debug_log("Battery status: {}", magic_enum::enum_name(battery->status));

    debug_log("Battery percentage: {}%", battery->percentage.value_or(0));

    if (battery->timeRemaining.has_value())
      debug_log("Battery time remaining: {}", SecondsToFormattedDuration(battery->timeRemaining.value()));
    else
      debug_log("Battery time remaining: N/A");
  } else
    debug_at(battery.error());

  if (Result<Output> primaryOutput = GetPrimaryOutput(cache)) {
    debug_log("Primary display ID: {}", primaryOutput->id);
    debug_log("Primary display resolution: {}x{}", primaryOutput->resolution.width, primaryOutput->resolution.height);
    debug_log("Primary display refresh rate: {:.2f}Hz", primaryOutput->refreshRate);
    debug_log("Primary display is primary: {}", primaryOutput->isPrimary);
  } else
    debug_at(primaryOutput.error());
#endif

  {
    const Config& config = Config::getInstance();
    SystemInfo    data(cache, config);

#if DRAC_ENABLE_WEATHER
    Result<Report> weatherReport;

    if (config.weather.enabled && config.weather.service == nullptr)
      weatherReport = Err({ Other, "Weather service is not configured" });
    else if (config.weather.enabled)
      weatherReport = config.weather.service->getWeatherInfo();
    else
      weatherReport = Err({ ApiUnavailable, "Weather is disabled" });
#endif

    if (doctorMode) {
      PrintDoctorReport(
#if DRAC_ENABLE_WEATHER
        weatherReport,
#endif
        data
      );

      return EXIT_SUCCESS;
    }

    String document;

#if DRAC_ENABLE_WEATHER
    Option<Report> weatherOption = None;

    if (weatherReport)
      weatherOption = *weatherReport;
    else if (weatherReport.error().code != ApiUnavailable)
      error_at(weatherReport.error());

    document = CreateUI(config, data, weatherOption);
#else
    document = CreateUI(config, data);
#endif

#ifdef _WIN32
    // For some reason, the box-drawing characters don't render correctly
    // in the console when using std::println/std::cout. Instead, we use
    // WriteConsoleW to more directly output to the console.
    WriteToConsole(document);
#else
    Println(document);
#endif
  }

  return EXIT_SUCCESS;
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
