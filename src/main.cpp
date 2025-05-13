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

#include "Core/SystemData.hpp"

#include "Config/Config.hpp"

#include "UI/UI.hpp"

#include "Util/Definitions.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

using util::types::i32, util::types::Exception;

fn main(const i32 argc, char* argv[]) -> i32 try {
#ifdef _WIN32
  winrt::init_apartment();
#endif

  {
    using argparse::ArgumentParser;

    ArgumentParser parser("draconis", "0.1.0");

    parser
      .add_argument("--log-level")
      .help("Set the log level")
      .default_value("info")
      .choices("debug", "info", "warn", "error");

    parser
      .add_argument("-V", "--verbose")
      .help("Enable verbose logging. Overrides --log-level.")
      .flag();

    if (Result<> result = parser.parse_args(argc, argv); !result) {
      error_at(result.error());
      return EXIT_FAILURE;
    }

    {
      using matchit::match, matchit::is, matchit::_;
      using util::logging::LogLevel;
      using enum util::logging::LogLevel;

      bool           verbose     = parser.get<bool>("-V").value_or(false) || parser.get<bool>("--verbose").value_or(false);
      Result<String> logLevelStr = verbose ? "debug" : parser.get<String>("--log-level");

      LogLevel minLevel = match(logLevelStr)(
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
