#include <argparse.hpp>            // argparse::ArgumentParser
#include <cstdlib>                 // EXIT_FAILURE, EXIT_SUCCESS
#include <ftxui/dom/elements.hpp>  // ftxui::{Element, hbox, vbox, text, separator, filler, etc.}
#include <ftxui/dom/node.hpp>      // ftxui::{Render}
#include <ftxui/screen/screen.hpp> // ftxui::{Screen, Dimension::Full}

#ifdef __cpp_lib_print
  #include <print> // std::print
#else
  #include <iostream> // std::cout
#endif

#include "Config/Config.hpp"

#include "Core/SystemData.hpp"

#include "UI/UI.hpp"

#include "Util/Definitions.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

using util::types::i32, util::types::Exception;

namespace {
  fn PrintDoctorReport(const os::SystemData& data) -> void {
    using util::types::u8, util::types::Vec, util::types::Pair;

    Vec<Pair<String, DracError>> failures;

    failures.reserve(12);

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
    if (!data.packageCount.has_value())
      failures.emplace_back("PackageCount", data.packageCount.error());
    if (!data.nowPlaying.has_value())
      failures.emplace_back("NowPlaying", data.nowPlaying.error());
    if (!data.weather.has_value())
      failures.emplace_back("Weather", data.weather.error());

    constexpr u8 totalPossibleReadouts = 12;

#ifdef __cpp_lib_print
    std::println("We've collected a total of {} readouts including {} failed read(s) and 0 read(s) resulting in a warning.\n", totalPossibleReadouts, failures.size());

    for (const auto& [readout, err] : failures)
      std::println("Readout \"{}\" failed with message: {} (code: {})", readout, err.message, err.code);
#else
    std::cout << "We've collected a total of " << totalPossibleReadouts
              << " readouts including " << failures.size()
              << " failed read(s) and 0 read(s) resulting in a warning.\n\n";

    for (const var& fail : failures)
      std::cout << "Readout \"" << fail.first << "\" failed with message: " << fail.second.message << "\n";

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

    ArgumentParser parser("draconis", DRACONISPLUSPLUS_VERSION);

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
      using matchit::match, matchit::is, matchit::_;
      using util::logging::LogLevel;
      using enum util::logging::LogLevel;

      const bool     verbose     = parser.get<bool>("-V").value_or(false) || parser.get<bool>("--verbose").value_or(false);
      Result<String> logLevelStr = verbose ? "debug" : parser.get<String>("--log-level");

      const LogLevel minLevel = match(logLevelStr)(
        is | "debug" = Debug,
        is | "info"  = Info,
        is | "warn"  = Warn,
        is | "error" = Error,
#ifndef NDEBUG
        is | _ = Debug
#else
        is | _ = Info
#endif
      );

      SetRuntimeLogLevel(minLevel);
    }
  }

  {
    const Config&        config = Config::getInstance();
    const os::SystemData data   = os::SystemData(config);

    if (doctorMode) {
      PrintDoctorReport(data);
      return EXIT_SUCCESS;
    }

    using ftxui::Element, ftxui::Screen, ftxui::Render;
    using ftxui::Dimension::Full, ftxui::Dimension::Fit;

    Element document = ui::CreateUI(config, data);

    Screen screen = Screen::Create(Full(), Fit(document));
    Render(screen, document);
    screen.Print();
  }

  // Running the program as part of the shell's startup will cut
  // off the last line of output, so we need to add a newline here.
#ifdef __cpp_lib_print
  std::println();
#else
  std::cout << '\n';
#endif
  return EXIT_SUCCESS;
} catch (const DracError& e) {
  error_at(e);
  return EXIT_FAILURE;
} catch (const Exception& e) {
  error_at(e);
  return EXIT_FAILURE;
}
