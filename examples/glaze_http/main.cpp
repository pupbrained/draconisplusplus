#define _WIN32_WINNT 0x0601

#include <asio/error.hpp> // asio::error::operation_aborted
#include <chrono>         // std::chrono::{minutes, steady_clock, time_point}
#include <csignal>        // SIGINT, SIGTERM, SIG_ERR, std::signal
#include <cstdlib>        // EXIT_FAILURE, EXIT_SUCCESS
#include <filesystem>     // std::filesystem::{path, weakly_canonical}
#include <fstream>        // std::ifstream
#include <thread>         // std::jthread

#ifdef DELETE
  #undef DELETE
#endif

#include <glaze/core/context.hpp>    // glz::error_ctx
#include <glaze/core/meta.hpp>       // glz::{meta, detail::Object}
#include <glaze/net/http_server.hpp> // glz::http_server
#include <matchit.hpp>               // matchit::impl::Overload
#include <mutex>                     // std::{mutex, unique_lock}
#include <optional>                  // std::optional
#include <utility>                   // std::move

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Weather.hpp>

#include <DracUtils/Definitions.hpp>
#include <DracUtils/Error.hpp>
#include <DracUtils/Logging.hpp>
#include <DracUtils/Types.hpp>

using namespace draconis::utils::types;
using namespace draconis::services::weather;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace fs = std::filesystem;

namespace {
  constexpr i16  port    = 3722;
  constexpr CStr index   = "examples/glaze_http/web/index.mustache";
  constexpr CStr styling = "examples/glaze_http/web/style.css";

  struct State {
#if DRAC_ENABLE_WEATHER
    mutable struct WeatherCache {
      std::optional<Result<Report>>         report;
      std::chrono::steady_clock::time_point lastChecked;
      mutable std::mutex                    mtx;
    } weatherCache;

    mutable UniquePointer<IWeatherService> weatherService;
#endif

    mutable struct HotReloading {
      std::filesystem::file_time_type lastWriteTime;
      mutable std::mutex              mtx;
    } hotReloading;
  };

  fn GetState() -> const State& {
    static const State STATE;

    return STATE;
  }

  fn get_latest_web_files_write_time() -> std::filesystem::file_time_type {
    fs::file_time_type tp1 = fs::exists(index) ? fs::last_write_time(index) : fs::file_time_type::min();
    fs::file_time_type tp2 = fs::exists(styling) ? fs::last_write_time(styling) : fs::file_time_type::min();

    return std::max(tp1, tp2);
  }

  fn readFile(const std::filesystem::path& path) -> Result<String> {
    if (!std::filesystem::exists(path))
      return Err(DracError(NotFound, std::format("File not found: {}", path.string())));

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
      return Err(DracError(IoError, std::format("Failed to open file: {}", path.string())));

    const usize size = std::filesystem::file_size(path);

    String result(size, '\0');

    file.read(result.data(), static_cast<std::streamsize>(size));

    return result;
  }

} // namespace

struct SystemProperty {
  String name;
  String value;
  String error;
  bool   hasError = false;

  SystemProperty(String name, String value)
    : name(std::move(name)), value(std::move(value)) {}

  SystemProperty(String name, const DracError& err)
    : name(std::move(name)), error(std::format("{} ({})", err.message, err.code)), hasError(true) {}
};

struct SystemInfo {
  Vec<SystemProperty> properties;
  String              version = DRAC_VERSION;
};

namespace glz {
  template <>
  struct meta<SystemProperty> {
    using T = SystemProperty;

    // clang-format off
    static constexpr glz::detail::Object value = glz::object(
      "name",     &T::name,
      "value",    &T::value,
      "error",    &T::error,
      "hasError", &T::hasError
    );
    // clang-format on
  };

  template <>
  struct meta<SystemInfo> {
    using T = SystemInfo;

    static constexpr glz::detail::Object value = glz::object("properties", &T::properties, "version", &T::version);
  };
} // namespace glz

fn main() -> i32 {
  glz::http_server server;

#if DRAC_ENABLE_WEATHER
  {
    GetState().weatherService = CreateWeatherService(Provider::METNO, Coords(40.71427, -74.00597), Unit::IMPERIAL);

    if (!GetState().weatherService)
      error_log("Error: Failed to initialize WeatherService.");
  }
#endif

  server.on_error([](const std::error_code errc, const std::source_location& loc) {
    if (errc != asio::error::operation_aborted)
      error_log("Server error at {}:{} -> {}", loc.file_name(), loc.line(), errc.message());
  });

  server.get("/hot_reload_check", [](const glz::request&, glz::response& res) {
    std::unique_lock lock(GetState().hotReloading.mtx);

    fs::file_time_type timePoint = GetState().hotReloading.lastWriteTime;

    res.body(std::to_string(timePoint.time_since_epoch().count()));
  });

  server.get("/style.css", [](const glz::request& req, glz::response& res) {
    info_log("Handling request for style.css from {}", req.remote_ip);

    Result<String> result = readFile(styling);

    if (result)
      res.header("Content-Type", "text/css; charset=utf-8")
        .header("Cache-Control", "no-cache, no-store, must-revalidate")
        .header("Pragma", "no-cache")
        .header("Expires", "0")
        .body(*result);
    else {
      error_log("Failed to serve style.css: {}", result.error().message);
      res.status(500).body("Internal Server Error: Could not load stylesheet.");
    }
  });

  server.get("/", [](const glz::request& req, glz::response& res) {
    info_log("Handling request from {}", req.remote_ip);

    SystemInfo sysInfo;

    {
      using draconis::core::system::System;
      using matchit::impl::Overload;
      using draconis::utils::logging::BytesToGiB;
      using enum draconis::utils::error::DracErrorCode;

      fn addProperty = Overload {
        [&](const String& name, const Result<String>& result) -> void {
          if (result)
            sysInfo.properties.emplace_back(name, *result);
          else if (result.error().code != NotSupported)
            sysInfo.properties.emplace_back(name, result.error());
        },
        [&](const String& name, const Result<ResourceUsage>& result) -> void {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{} / {}", BytesToGiB(result->usedBytes), BytesToGiB(result->totalBytes)));
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
#if DRAC_ENABLE_NOWPLAYING
        [&](const String& name, const Result<MediaInfo>& result) -> void {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{} - {}", result->title.value_or("Unknown Title"), result->artist.value_or("Unknown Artist")));
          else if (result.error().code == NotFound)
            sysInfo.properties.emplace_back(name, "No media playing");
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
#endif
#if DRAC_ENABLE_WEATHER
        [&](const String& name, const Result<Report>& result) -> void {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{}°F, {}", std::lround(result->temperature), result->description));
          else if (result.error().code == NotFound)
            sysInfo.properties.emplace_back(name, "No weather data available");
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
#endif
      };

      addProperty("OS Version", System::getOSVersion());
      addProperty("Kernel Version", System::getKernelVersion());
      addProperty("Host", System::getHost());
      addProperty("Shell", System::getShell());
      addProperty("Desktop Environment", System::getDesktopEnvironment());
      addProperty("Window Manager", System::getWindowManager());
      addProperty("CPU Model", System::getCPUModel());
      addProperty("GPU Model", System::getGPUModel());
      addProperty("Memory", System::getMemInfo());
      addProperty("Disk Usage", System::getDiskUsage());
#if DRAC_ENABLE_NOWPLAYING
      addProperty("Now Playing", System::getNowPlaying());
#endif
#if DRAC_ENABLE_WEATHER
      {
        using namespace std::chrono;

        Result<Report> weatherResultToAdd;

        std::unique_lock<std::mutex> lock(GetState().weatherCache.mtx);

        time_point now = steady_clock::now();

        bool needsFetch = true;

        if (GetState().weatherCache.report.has_value() && now - GetState().weatherCache.lastChecked < minutes(10)) {
          info_log("Using cached weather data.");
          weatherResultToAdd = *GetState().weatherCache.report;
          needsFetch         = false;
        }

        if (needsFetch) {
          info_log("Fetching new weather data...");
          if (GetState().weatherService) {
            Result<Report> fetchedReport        = GetState().weatherService->getWeatherInfo();
            GetState().weatherCache.report      = fetchedReport;
            GetState().weatherCache.lastChecked = now;
            weatherResultToAdd                  = fetchedReport;
          } else {
            error_log("Weather service is not initialized. Cannot fetch new data.");
            Result<Report> errorReport          = Err(DracError(ApiUnavailable, "Weather service not initialized"));
            GetState().weatherCache.report      = errorReport;
            GetState().weatherCache.lastChecked = now;
            weatherResultToAdd                  = errorReport;
          }
        }

        lock.unlock();

        addProperty("Weather", weatherResultToAdd);
      }
#endif
    }

    Result<String> htmlTemplate = readFile(index);

    if (!htmlTemplate) {
      error_log("Failed to read HTML template: {}", htmlTemplate.error().message);
      res.status(500).body("Internal Server Error: Template file not found.");
      return;
    }

    if (Result<String, glz::error_ctx> result = glz::stencil(*htmlTemplate, sysInfo))
      res.header("Content-Type", "text/html; charset=utf-8")
        .header("Cache-Control", "no-cache, no-store, must-revalidate")
        .header("Pragma", "no-cache")
        .header("Expires", "0")
        .body(*result);
    else {
      String errorString = glz::format_error(result.error(), *htmlTemplate);
      error_log("Failed to render stencil template:\n{}", errorString);
      res.status(500).body("Internal Server Error: Template rendering failed.");
    }
  });

  GetState().hotReloading.lastWriteTime = get_latest_web_files_write_time();

  std::jthread fileWatcherThread([](const std::stop_token& stopToken) {
    while (!stopToken.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      auto latestTime = get_latest_web_files_write_time();

      std::unique_lock lock(GetState().hotReloading.mtx);
      if (latestTime > GetState().hotReloading.lastWriteTime) {
        info_log("Web file change detected, updating timestamp.");
        GetState().hotReloading.lastWriteTime = latestTime;
      }
    }
    info_log("File watcher thread stopped.");
  });

  server.bind(port);
  server.start();

  info_log("Server started at http://localhost:{}. Press Ctrl+C to exit.", port);

  {
    using namespace asio;

    io_context signalContext;

    signal_set signals(signalContext, SIGINT, SIGTERM);

    signals.async_wait([&](const error_code& error, i32 signal_number) {
      if (!error) {
        info_log("\nShutdown signal ({}) received. Stopping server...", signal_number);
        server.stop();
        signalContext.stop();
      }
    });

    signalContext.run();
  }

  info_log("Server stopped. Exiting.");
  return EXIT_SUCCESS;
}
