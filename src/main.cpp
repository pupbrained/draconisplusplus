#include <ctime>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <future>
#include <string>

#include "config/config.h"
#include "ftxui/screen/color.hpp"
#include "os/os.h"

constexpr const bool SHOW_ICONS = true;

struct BytesToGiB {
  u64 value;
};

// 1024^3 (size of 1 GiB)
constexpr u64 GIB = 1'073'741'824;

template <>
struct fmt::formatter<BytesToGiB> : fmt::formatter<double> {
  template <typename FmtCtx>
  constexpr fn format(const BytesToGiB& BTG, FmtCtx& ctx) const -> typename FmtCtx::iterator {
    typename FmtCtx::iterator out = fmt::formatter<double>::format(static_cast<double>(BTG.value) / GIB, ctx);

    *out++ = 'G';
    *out++ = 'i';
    *out++ = 'B';

    return out;
  }
};

namespace {
  fn GetDate() -> std::string {
    // Get current local time
    std::time_t now = std::time(nullptr);
    std::tm     localTime;

#ifdef __WIN32__
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

    // Append appropriate suffix for the date
    if (date.ends_with("1") && date != "11")
      date += "st";
    else if (date.ends_with("2") && date != "12")
      date += "nd";
    else if (date.ends_with("3") && date != "13")
      date += "rd";
    else
      date += "th";

    return fmt::format("{:%B} {}", localTime, date);
  }

  struct SystemData {
    std::string                     date;
    std::string                     host;
    std::string                     kernel_version;
    std::string                     os_version;
    std::expected<u64, std::string> mem_info;
    std::string                     desktop_environment;
    std::string                     window_manager;
    std::optional<std::string>      now_playing;
    std::optional<WeatherOutput>    weather_info;

    static fn fetchSystemData(const Config& config) -> SystemData {
      SystemData data;

      std::launch launchPolicy = std::launch::async | std::launch::deferred;

      if (std::thread::hardware_concurrency() >= 8)
        launchPolicy = std::launch::async;

      // Launch async tasks
      std::future<string>                     futureDate   = std::async(launchPolicy, GetDate);
      std::future<string>                     futureHost   = std::async(launchPolicy, GetHost);
      std::future<string>                     futureKernel = std::async(launchPolicy, GetKernelVersion);
      std::future<string>                     futureOs     = std::async(launchPolicy, GetOSVersion);
      std::future<std::expected<u64, string>> futureMem    = std::async(launchPolicy, GetMemInfo);
      std::future<string>                     futureDe     = std::async(launchPolicy, GetDesktopEnvironment);
      std::future<string>                     futureWm     = std::async(launchPolicy, GetWindowManager);

      std::future<WeatherOutput> futureWeather;

      if (config.weather.get().enabled)
        futureWeather = std::async(std::launch::async, [&config]() { return config.weather.get().getWeatherInfo(); });

      std::future<std::string> futureNowPlaying;

      if (config.now_playing.get().enabled)
        futureNowPlaying = std::async(std::launch::async, GetNowPlaying);

      // Collect results
      data.date                = futureDate.get();
      data.host                = futureHost.get();
      data.kernel_version      = futureKernel.get();
      data.os_version          = futureOs.get();
      data.mem_info            = futureMem.get();
      data.desktop_environment = futureDe.get();
      data.window_manager      = futureWm.get();

      if (config.weather.get().enabled)
        data.weather_info = futureWeather.get();

      if (config.now_playing.get().enabled)
        data.now_playing = futureNowPlaying.get();

      return data;
    }
  };

  using namespace ftxui;

  fn CreateColorCircles() -> Element {
    Elements circles;

    for (int i = 0; i < 16; ++i)
      circles.push_back(hbox({
        text("◯") | bold | color(Color::Palette256(i)),
        text(" "),
      }));

    return hbox(circles);
  }

  fn SystemInfoBox(const Config& config, const SystemData& data) -> Element {
    // Fetch data
    const string& name              = config.general.get().name.get();
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

    const Color::Palette16 labelColor  = Color::Yellow;
    const Color::Palette16 valueColor  = Color::White;
    const Color::Palette16 borderColor = Color::GrayLight;
    const Color::Palette16 iconColor   = Color::Cyan;

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

    if (!data.os_version.empty())
      content.push_back(createRow(osIcon, "OS", data.os_version));

    if (data.mem_info.has_value())
      content.push_back(createRow(memoryIcon, "RAM", fmt::format("{:.2f}", BytesToGiB { data.mem_info.value() })));
    else
      ERROR_LOG("Failed to get memory info: {}", data.mem_info.error());

    content.push_back(separator() | color(borderColor));

    if (!data.desktop_environment.empty() && data.desktop_environment != data.window_manager)
      content.push_back(createRow(" 󰇄  ", "DE", data.desktop_environment));

    if (!data.window_manager.empty())
      content.push_back(createRow("   ", "WM", data.window_manager));

    // Now Playing row
    if (nowPlayingEnabled && data.now_playing.has_value()) {
      content.push_back(separator() | color(borderColor));
      content.push_back(hbox({
        text(musicIcon) | color(iconColor),
        text("Music") | color(labelColor),
        text(" "),
        filler(),
        text(
          data.now_playing.value().length() > 30 ? data.now_playing.value().substr(0, 30) + "..."
                                                 : data.now_playing.value()
        ) |
          color(Color::Magenta),
        text(" "),
      }));
    }

    return vbox(content) | borderRounded | color(Color::White);
  }
}

fn main() -> i32 {
  const Config& config = Config::getInstance();

  SystemData data = SystemData::fetchSystemData(config);

  Element document = hbox({ SystemInfoBox(config, data), filler() });

  Screen screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
  Render(screen, document);
  screen.Print();

  return 0;
}
