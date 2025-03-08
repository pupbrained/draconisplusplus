#include <ctime>
#include <expected>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <future>
#include <string>
#include <utility>
#include <variant>

#include "config/config.h"
#include "ftxui/screen/color.hpp"
#include "os/os.h"

constexpr bool SHOW_ICONS = true;

struct BytesToGiB {
  u64 value;
};

// 1024^3 (size of 1 GiB)
constexpr u64 GIB = 1'073'741'824;

template <>
struct fmt::formatter<BytesToGiB> : fmt::formatter<double> {
  template <typename FmtCtx>
  constexpr fn format(const BytesToGiB& BTG, FmtCtx& ctx) const -> typename FmtCtx::iterator {
    // Format as double with GiB suffix, no space
    return fmt::format_to(ctx.out(), "{:.2f}GiB", static_cast<double>(BTG.value) / GIB);
  }
};

namespace {
  fn GetDate() -> std::string {
    // Get current local time
    const std::time_t now = std::time(nullptr);
    std::tm           localTime;

#ifdef _WIN32
    if (localtime_s(&localTime, &now) != 0)
      ERROR_LOG("localtime_s failed");
#else
    if (localtime_r(&now, &localTime) == nullptr)
      ERROR_LOG("localtime_r failed");
#endif

    // Format the date using fmt::format
    std::string date = fmt::format("{:%e}", localTime);

    // Remove leading whitespace
    if (!date.empty() && std::isspace(date.front()))
      date.erase(date.begin());

    // Append appropriate suffix for the datE
    if (date.back() == '1' && date != "11")
      date += "st";
    else if (date.back() == '2' && date != "12")
      date += "nd";
    else if (date.back() == '3' && date != "13")
      date += "rd";
    else
      date += "th";

    return fmt::format("{:%B} {}", localTime, date);
  }

  struct SystemData {
    std::string                                           date;
    std::string                                           host;
    std::string                                           kernel_version;
    std::expected<string, string>                         os_version;
    std::expected<u64, std::string>                       mem_info;
    std::optional<string>                                 desktop_environment;
    std::string                                           window_manager;
    std::optional<std::expected<string, NowPlayingError>> now_playing;
    std::optional<WeatherOutput>                          weather_info;
    u64                                                   disk_used;
    u64                                                   disk_total;
    std::string                                           shell;

    static fn fetchSystemData(const Config& config) -> SystemData {
      SystemData data;

      // Single-threaded execution for core system info (faster on Windows)
      data.date           = GetDate();
      data.host           = GetHost();
      data.kernel_version = GetKernelVersion();
      data.os_version     = GetOSVersion();
      data.mem_info       = GetMemInfo();

      // Desktop environment info
      data.desktop_environment = GetDesktopEnvironment();
      data.window_manager      = GetWindowManager();

      // Parallel execution for disk/shell only
      auto diskShell = std::async(std::launch::async, [] {
        auto [used, total] = GetDiskUsage();
        return std::make_tuple(used, total, GetShell());
      });

      // Conditional tasks
      std::future<WeatherOutput>                          weather;
      std::future<std::expected<string, NowPlayingError>> nowPlaying;

      if (config.weather.get().enabled)
        weather = std::async(std::launch::async, [&config] { return config.weather.get().getWeatherInfo(); });

      if (config.now_playing.get().enabled)
        nowPlaying = std::async(std::launch::async, GetNowPlaying);

      // Get remaining results
      auto [used, total, shell] = diskShell.get();
      data.disk_used            = used;
      data.disk_total           = total;
      data.shell                = shell;

      if (weather.valid())
        data.weather_info = weather.get();
      if (nowPlaying.valid())
        data.now_playing = nowPlaying.get();

      return data;
    }
  };

  using namespace ftxui;

  fn CreateColorCircles() -> Element {
    Elements circles(16);

    std::generate_n(circles.begin(), 16, [colorIndex = 0]() mutable {
      return hbox({ text("◯") | bold | color(static_cast<Color::Palette256>(colorIndex++)), text(" ") });
    });

    return hbox(circles);
  }

  fn SystemInfoBox(const Config& config, const SystemData& data) -> Element {
    // Fetch data
    const string& name              = config.general.get().name;
    const Weather weather           = config.weather.get();
    const bool    nowPlayingEnabled = config.now_playing.get().enabled;

    const char *calendarIcon = "", *hostIcon = "", *kernelIcon = "", *osIcon = "", *memoryIcon = "", *weatherIcon = "",
               *musicIcon = "";

    if (SHOW_ICONS) {
      calendarIcon = "   ";
      hostIcon     = " 󰌢  ";
      kernelIcon   = "   ";
      osIcon       = "   ";
      memoryIcon   = "   ";
      weatherIcon  = "   ";
      musicIcon    = "   ";
    }

    constexpr Color::Palette16 labelColor  = Color::Yellow;
    constexpr Color::Palette16 valueColor  = Color::White;
    constexpr Color::Palette16 borderColor = Color::GrayLight;
    constexpr Color::Palette16 iconColor   = Color::Cyan;

    Elements content;

    content.push_back(text("   Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(borderColor));
    content.push_back(hbox({
      text("   ") | color(iconColor), // Palette icon
      CreateColorCircles(),
    }));
    content.push_back(separator() | color(borderColor));

    // Helper function for aligned rows
    fn createRow = [&](const std::string& icon, const std::string& label, const std::string& value) {
      return hbox({
        text(icon) | color(iconColor),
        text(label) | color(labelColor),
        filler(),
        text(value) | color(valueColor),
        text(" "),
      });
    };

    // System info rows
    content.push_back(createRow(calendarIcon, "Date", data.date));

    // Weather row
    if (weather.enabled && data.weather_info.has_value()) {
      const WeatherOutput& weatherInfo = data.weather_info.value();

      if (weather.show_town_name)
        content.push_back(hbox({
          text(weatherIcon) | color(iconColor),
          text("Weather") | color(labelColor),
          filler(),

          hbox({
            text(fmt::format("{}°F ", std::lround(weatherInfo.main.temp))),
            text("in "),
            text(weatherInfo.name),
            text(" "),
          }) |
            color(valueColor),
        }));
      else
        content.push_back(hbox({
          text(weatherIcon) | color(iconColor),
          text("Weather") | color(labelColor),
          filler(),

          hbox({
            text(fmt::format("{}°F, {}", std::lround(weatherInfo.main.temp), weatherInfo.weather[0].description)),
            text(" "),
          }) |
            color(valueColor),
        }));
    }

    content.push_back(separator() | color(borderColor));

    if (!data.host.empty())
      content.push_back(createRow(hostIcon, "Host", data.host));

    if (!data.kernel_version.empty())
      content.push_back(createRow(kernelIcon, "Kernel", data.kernel_version));

    if (data.os_version.has_value())
      content.push_back(createRow(osIcon, "OS", *data.os_version));
    else
      ERROR_LOG("Failed to get OS version: {}", data.os_version.error());

    if (data.mem_info.has_value())
      content.push_back(createRow(memoryIcon, "RAM", fmt::format("{}", BytesToGiB { *data.mem_info })));
    else
      ERROR_LOG("Failed to get memory info: {}", data.mem_info.error());

    // Add Disk usage row
    content.push_back(
      createRow(" 󰋊  ", "Disk", fmt::format("{}/{}", BytesToGiB { data.disk_used }, BytesToGiB { data.disk_total }))
    );

    content.push_back(createRow("   ", "Shell", data.shell));

    content.push_back(separator() | color(borderColor));

    if (data.desktop_environment.has_value() && *data.desktop_environment != data.window_manager)
      content.push_back(createRow(" 󰇄  ", "DE", *data.desktop_environment));

    if (!data.window_manager.empty())
      content.push_back(createRow("   ", "WM", data.window_manager));

    // Now Playing row
    if (nowPlayingEnabled && data.now_playing.has_value()) {
      const std::expected<string, NowPlayingError>& nowPlayingResult = *data.now_playing;

      if (nowPlayingResult.has_value()) {
        const std::string& npText = *nowPlayingResult;

        content.push_back(separator() | color(borderColor));
        content.push_back(hbox({
          text(musicIcon) | color(iconColor),
          text("Playing") | color(labelColor),
          text(" "),
          filler(),
          paragraph(npText) | color(Color::Magenta) | size(WIDTH, LESS_THAN, 30),
          text(" "),
        }));
      } else {
        const NowPlayingError& error = nowPlayingResult.error();

        if (std::holds_alternative<NowPlayingCode>(error))
          switch (std::get<NowPlayingCode>(error)) {
            case NowPlayingCode::NoPlayers:
              DEBUG_LOG("No players found");
              break;
            case NowPlayingCode::NoActivePlayer:
              DEBUG_LOG("No active player found");
              break;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcovered-switch-default"
            default:
              std::unreachable();
#pragma clang diagnostic pop
          }

#ifdef __linux__
        if (std::holds_alternative<LinuxError>(error))
          DEBUG_LOG("DBus error: {}", std::get<LinuxError>(error));
#endif

#ifdef _WIN32
        if (std::holds_alternative<WindowsError>(error))
          DEBUG_LOG("WinRT error: {}", to_string(std::get<WindowsError>(error).message()));
#endif
      }
    }

    return vbox(content) | borderRounded | color(Color::White);
  }
}

fn main() -> i32 {
  const Config&    config = Config::getInstance();
  const SystemData data   = SystemData::fetchSystemData(config);

  Element document = vbox({ hbox({ SystemInfoBox(config, data), filler() }), text("") });

  Screen screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();

  return 0;
}
