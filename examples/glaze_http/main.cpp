#include <asio/error.hpp>            // asio::error::operation_aborted
#include <chrono>                    // std::chrono::{minutes, steady_clock, time_point}
#include <csignal>                   // SIGINT, SIGTERM, SIG_ERR, std::signal
#include <cstdlib>                   // EXIT_FAILURE, EXIT_SUCCESS
#include <fstream>                   // std::ifstream
#include <magic_enum/magic_enum.hpp> // magic_enum::enum_name

#ifdef DELETE
  #undef DELETE
#endif

#ifdef fn
  #undef fn
#endif

#include <glaze/core/context.hpp>    // glz::error_ctx
#include <glaze/core/meta.hpp>       // glz::{meta, detail::Object}
#include <glaze/net/http_server.hpp> // glz::http_server
#include <matchit.hpp>               // matchit::impl::Overload
#include <mutex>                     // std::{mutex, unique_lock}
#include <optional>                  // std::optional
#include <utility>                   // std::move

#ifndef fn
  #define fn auto
#endif

#include <Drac++/Core/System.hpp>
#include <Drac++/Services/Weather.hpp>

#include <Drac++/Utils/CacheManager.hpp>
#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::utils::types;
using namespace draconis::services::weather;
using draconis::utils::error::DracError;
using enum draconis::utils::error::DracErrorCode;

namespace {
  constexpr i16   port        = 3722;
  constexpr PCStr indexFile   = "examples/glaze_http/web/index.mustache";
  constexpr PCStr stylingFile = "examples/glaze_http/web/style.css";

  struct State {
#if DRAC_ENABLE_WEATHER
    mutable struct WeatherCache {
      std::optional<Result<Report>>         report;
      std::chrono::steady_clock::time_point lastChecked;
      mutable std::mutex                    mtx;
    } weatherCache;

    mutable UniquePointer<IWeatherService> weatherService;
#endif
  };

  fn GetState() -> const State& {
    static const State STATE;

    return STATE;
  }

  fn readFile(const std::filesystem::path& path) -> Result<String> {
    if (!std::filesystem::exists(path))
      ERR_FMT(NotFound, "File not found: {}", path.string());

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file)
      ERR_FMT(IoError, "Failed to open file: {}", path.string());

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
    : name(std::move(name)), error(std::format("{} ({})", err.message, magic_enum::enum_name(err.code))), hasError(true) {}
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
    GetState().weatherService = CreateWeatherService(Provider::MetNo, Coords(40.71427, -74.00597), UnitSystem::Imperial);

    if (!GetState().weatherService)
      error_log("Error: Failed to initialize WeatherService.");
  }
#endif

  server.on_error([](const std::error_code errc, const std::source_location& loc) {
    if (errc != asio::error::operation_aborted)
      error_log("Server error at {}:{} -> {}", loc.file_name(), loc.line(), errc.message());
  });

  server.get("/style.css", [](const glz::request& req, glz::response& res) {
    info_log("Handling request for style.css from {}", req.remote_ip);

    Result<String> result = readFile(stylingFile);

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

    draconis::utils::cache::CacheManager cacheManager;

    {
      using namespace draconis::core::system;
      using matchit::impl::Overload;
      using enum draconis::utils::error::DracErrorCode;

      fn addProperty = Overload {
        [&](const String& name, const Result<String>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, *result);
          else if (result.error().code != NotSupported)
            sysInfo.properties.emplace_back(name, result.error());
        },
        [&](const String& name, const Result<OSInfo>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{} {}", result->name, result->version));
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
        [&](const String& name, const Result<ResourceUsage>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{} / {}", BytesToGiB(result->usedBytes), BytesToGiB(result->totalBytes)));
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
#if DRAC_ENABLE_NOWPLAYING
        [&](const String& name, const Result<MediaInfo>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{} - {}", result->title.value_or("Unknown Title"), result->artist.value_or("Unknown Artist")));
          else if (result.error().code == NotFound)
            sysInfo.properties.emplace_back(name, "No media playing");
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
#endif
#if DRAC_ENABLE_WEATHER
        [&](const String& name, const Result<Report>& result) -> Unit {
          if (result)
            sysInfo.properties.emplace_back(name, std::format("{}°F, {}", std::lround(result->temperature), result->description));
          else if (result.error().code == NotFound)
            sysInfo.properties.emplace_back(name, "No weather data available");
          else
            sysInfo.properties.emplace_back(name, result.error());
        },
#endif
      };

      addProperty("OS", GetOperatingSystem(cacheManager));
      addProperty("Kernel Version", GetKernelVersion(cacheManager));
      addProperty("Host", GetHost(cacheManager));
      addProperty("Shell", GetShell(cacheManager));
      addProperty("Desktop Environment", GetDesktopEnvironment(cacheManager));
      addProperty("Window Manager", GetWindowManager(cacheManager));
      addProperty("CPU Model", GetCPUModel(cacheManager));
      addProperty("GPU Model", GetGPUModel(cacheManager));
      addProperty("Memory", GetMemInfo(cacheManager));
      addProperty("Disk Usage", GetDiskUsage(cacheManager));
#if DRAC_ENABLE_NOWPLAYING
      addProperty("Now Playing", GetNowPlaying());
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
            Result<Report> errorReport          = Err({ ApiUnavailable, "Weather service not initialized" });
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

    Result<String> htmlTemplate = readFile(indexFile);

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
      error_log("Failed to render stencil template:\n{}", glz::format_error(result.error(), *htmlTemplate));
      res.status(500).body("Internal Server Error: Template rendering failed.");
    }
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
