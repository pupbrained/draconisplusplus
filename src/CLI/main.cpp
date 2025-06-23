#include <argparse.hpp>            // argparse::ArgumentParser
#include <cstdlib>                 // EXIT_FAILURE, EXIT_SUCCESS
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}
#include <matchit.hpp>             // matchit::{match, is, _}

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout
#endif

#include <Drac++/Services/Packages.hpp>
#include <Drac++/Services/Weather.hpp>

#include <Drac++/Utils/Definitions.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

#include "Config/Config.hpp"
#include "Core/SystemInfo.hpp"
#include "UI/UI.hpp"

using namespace draconis::utils::types;
using namespace draconis::core::system;
using namespace draconis::config;
using namespace draconis::ui;

using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

using draconis::services::weather::Report;

namespace {
#if DRAC_ENABLE_WEATHER
  fn PrintDoctorReport(const SystemInfo& data, const Result<Report>& weather) -> void {
#else
  fn PrintDoctorReport(const SystemInfo& data) -> void {
#endif
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

#ifdef __cpp_lib_print
    std::println("{}", summary);
#else
    std::cout << summary;
#endif

    for (const auto& [readout, err] : failures) {
      const String failureLine = std::format(
        "Readout \"{}\" failed: {} (code: {})\n",
        readout,
        err.message,
        err.code
      );

#ifdef __cpp_lib_print
      std::println("{}", failureLine);
#else
      std::cout << failureLine;
#endif
    }

#ifndef __cpp_lib_print
    std::cout << std::flush;
#endif
  }

} // namespace

fn main(const i32 argc, char* argv[]) -> i32 try {
#ifdef _WIN32
  winrt::init_apartment();
#endif

  bool doctorMode = false;

  {
    using argparse::ArgumentParser;

    ArgumentParser parser("draconis", DRAC_VERSION);

    parser
      .add_argument("--log-level")
      .help("Set the log level")
      .default_value("info")
      .choices("debug", "info", "warn", "error");

    parser
      .add_argument("-V", "--verbose")
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    parser
      .add_argument("-d", "--doctor")
      .help("Reports any failed readouts and their error messages.")
      .flag();

    if (Result result = parser.parse_args(argc, argv); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    doctorMode = parser.get<bool>("-d").value_or(false) || parser.get<bool>("--doctor").value_or(false);

    {
      using draconis::utils::logging::LogLevel;
      using matchit::match, matchit::is, matchit::_;
      using enum draconis::utils::logging::LogLevel;

      const bool     verbose     = parser.get<bool>("-V").value_or(false) || parser.get<bool>("--verbose").value_or(false);
      Result<String> logLevelStr = verbose ? "debug" : parser.get<String>("--log-level");

      const LogLevel minLevel = match(logLevelStr)(
        is | "debug" = Debug,
        is | "info"  = Info,
        is | "warn"  = Warn,
        is | "error" = Error
      );

      SetRuntimeLogLevel(minLevel);
    }
  }

  {
    using namespace ftxui;
    using namespace ftxui::Dimension;
    using matchit::match, matchit::is, matchit::_;

    const Config&    config = Config::getInstance();
    const SystemInfo data(config);

    Result<Report> weatherReport = config.weather.service->getWeatherInfo();

    if (doctorMode) {
#if DRAC_ENABLE_WEATHER
      PrintDoctorReport(data, weatherReport);
#else
      PrintDoctorReport(data);
#endif
      return EXIT_SUCCESS;
    }

    Element document;

    if (weatherReport) {
      document = CreateUI(config, data, *weatherReport);
    } else {
      error_at(weatherReport.error());
      document = CreateUI(config, data, None);
    }

    Screen screen = Screen::Create(Full(), Fit(document));
    Render(screen, document);
    screen.Print();
  }

  if (Result<Vec<Display>> displays = GetDisplays()) {
    for (const Display& display : *displays) {
      info_log("Display ID: {}", display.id);
      info_log("Display resolution: {}x{}", display.resolution.width, display.resolution.height);
      info_log("Display refresh rate: {}Hz", display.refreshRate);
      info_log("Display is primary: {}", display.isPrimary);
    }
  } else {
    debug_at(displays.error());
  }

  if (Result<Vec<NetworkInterface>> networkInterfaces = GetNetworkInterfaces()) {
    for (const NetworkInterface& networkInterface : *networkInterfaces) {
      info_log("Network interface: {}", networkInterface.name);
      info_log("Network interface IPv4 address: {}", networkInterface.ipv4Address.value_or("N/A"));
      info_log("Network interface IPv6 address: {}", networkInterface.ipv6Address.value_or("N/A"));
      info_log("Network interface MAC address: {}", networkInterface.macAddress.value_or("N/A"));
      info_log("Network interface is up: {}", networkInterface.isUp);
      info_log("Network interface is loopback: {}", networkInterface.isLoopback);
    }
  } else {
    debug_at(networkInterfaces.error());
  }

  // Running the program as part of the shell's startup will cut
  // off the last line of output, so we need to add a newline here.
#ifdef __cpp_lib_print
  std::println();
#else
  std::cout << '\n';
#endif

  return EXIT_SUCCESS;
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
