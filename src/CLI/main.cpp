#include <cstdlib>                 // EXIT_FAILURE, EXIT_SUCCESS
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}
#include <matchit.hpp>             // matchit::{match, is, _}

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Packages.hpp>

#if DRAC_ENABLE_WEATHER
  #include <Drac++/Services/Weather.hpp>
#endif

#include <Drac++/Utils/ArgumentParser.hpp>
#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Definitions.hpp>
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

using draconis::utils::cache::CacheManager;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

#if DRAC_ENABLE_WEATHER
using draconis::services::weather::Report;
#endif

namespace {
  fn PrintDoctorReport(
#if DRAC_ENABLE_WEATHER
    const Result<Report>& weather,
#endif
    const SystemInfo& data
  ) -> void {
    Vec<Pair<String, DracError>> failures;

    constexpr u8 totalPossibleReadouts = 10
#if DRAC_ENABLE_PACKAGECOUNT
      + 1
#endif
#if DRAC_ENABLE_NOWPLAYING
      + 1
#endif
#if DRAC_ENABLE_WEATHER
      + 1
#endif
      ;

    failures.reserve(totalPossibleReadouts);

    if (!data.date.has_value())
      failures.emplace_back("Date", data.date.error());
    if (!data.host.has_value())
      failures.emplace_back("Host", data.host.error());
    if (!data.kernelVersion.has_value())
      failures.emplace_back("KernelVersion", data.kernelVersion.error());
    if (!data.osVersion.has_value())
      failures.emplace_back("OSVersion", data.osVersion.error());
    if (!data.memInfo.has_value())
      failures.emplace_back("MemoryInfo", data.memInfo.error());
    if (!data.desktopEnv.has_value())
      failures.emplace_back("DesktopEnvironment", data.desktopEnv.error());
    if (!data.windowMgr.has_value())
      failures.emplace_back("WindowManager", data.windowMgr.error());
    if (!data.diskUsage.has_value())
      failures.emplace_back("DiskUsage", data.diskUsage.error());
    if (!data.shell.has_value())
      failures.emplace_back("Shell", data.shell.error());
    if (!data.uptime.has_value())
      failures.emplace_back("Uptime", data.uptime.error());
#if DRAC_ENABLE_PACKAGECOUNT
    if (!data.packageCount.has_value())
      failures.emplace_back("PackageCount", data.packageCount.error());
#endif
#if DRAC_ENABLE_NOWPLAYING
    if (!data.nowPlaying.has_value())
      failures.emplace_back("NowPlaying", data.nowPlaying.error());
#endif
#if DRAC_ENABLE_WEATHER
    if (!weather.has_value())
      failures.emplace_back("Weather", weather.error());
#endif

    const String summary = std::format(
      "We've collected a total of {} readouts including {} failed read{}.\n\n",
      totalPossibleReadouts,
      failures.size(),
      failures.size() == 1 ? "" : "s"
    );

    Print(summary);

    for (const auto& [readout, err] : failures) {
      const String failureLine = std::format(
        "Readout \"{}\" failed: {} (code: {})\n",
        readout,
        err.message,
        err.code
      );

      Print(failureLine);
    }

    // Flush is handled automatically by the Print function
  }
} // namespace

fn main(const i32 argc, char* argv[]) -> i32 try {
#ifdef _WIN32
  winrt::init_apartment();
#endif

  bool doctorMode = false;

  {
    using draconis::utils::argparse::ArgumentParser;

    ArgumentParser parser("draconis", DRAC_VERSION);

    parser
      .addArgument({ "-V", "--verbose" })
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    parser
      .addArgument({ "-d", "--doctor" })
      .help("Reports any failed readouts and their error messages.")
      .flag();

    parser
      .addArgument({ "--log-level" })
      .help("Set the minimum log level. Defaults to info.")
      .defaultValue(LogLevel::Info);

    if (Result result = parser.parseArgs(std::span(argv, static_cast<usize>(argc))); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    doctorMode = parser.get<bool>("-d") || parser.get<bool>("--doctor");

    {
      const bool verbose = parser.get<bool>("-V") || parser.get<bool>("--verbose");

      LogLevel minLevel = verbose ? LogLevel::Debug : parser.getEnum<LogLevel>("--log-level");

      SetRuntimeLogLevel(minLevel);
    }
  }

  CacheManager cache;
  cache.setGlobalPolicy({ .location = draconis::utils::cache::CacheLocation::Persistent, .ttl = std::chrono::hours(12) });

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

  if (Result<Battery> battery = GetBatteryInfo()) {
    using matchit::match, matchit::is, matchit::_;

    debug_log(
      "Battery status: {}",
      match(battery->status)(
        is | Battery::Status::Charging    = "Charging",
        is | Battery::Status::Discharging = "Discharging",
        is | Battery::Status::Full        = "Full",
        is | Battery::Status::Unknown     = "Unknown",
        is | Battery::Status::NotPresent  = "Not Present"
      )
    );

    debug_log("Battery percentage: {}%", battery->percentage.value_or(0));

    if (battery->timeRemaining.has_value())
      debug_log("Battery time remaining: {}", SecondsToFormattedDuration(battery->timeRemaining.value()));
    else
      debug_log("Battery time remaining: N/A");
  } else
    debug_at(battery.error());

  {
    using namespace ftxui;
    using namespace ftxui::Dimension;
    using matchit::match, matchit::is, matchit::_;

    const Config& config = Config::getInstance();
    SystemInfo    data(cache, config);

    if (data.primaryDisplay) {
      debug_log("Display ID: {}", data.primaryDisplay->id);
      debug_log("Display resolution: {}x{}", data.primaryDisplay->resolution.width, data.primaryDisplay->resolution.height);
      debug_log("Display refresh rate: {}Hz", data.primaryDisplay->refreshRate);
      debug_log("Display is primary: {}", data.primaryDisplay->isPrimary);
    } else
      debug_at(data.primaryDisplay.error());

#if DRAC_ENABLE_WEATHER
    Result<Report> weatherReport;

    if (config.weather.enabled && config.weather.service == nullptr)
      weatherReport = Err(DracError(Other, "Weather service is not configured"));
    else if (config.weather.enabled)
      weatherReport = config.weather.service->getWeatherInfo();
    else
      weatherReport = Err(DracError(ApiUnavailable, "Weather is disabled"));
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

    Element document;

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

    Screen screen = Screen::Create(Full(), Fit(document));
    Render(screen, document);
    screen.Print();
  }

  // Running the program as part of the shell's startup will cut
  // off the last line of output, so we need to add a newline here.
  Println();

  return EXIT_SUCCESS;
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
