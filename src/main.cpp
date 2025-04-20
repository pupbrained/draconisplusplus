#include <ctime>
#include <expected>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>
#include <future>
#include <string>
#include <utility>
#include <variant>

#include "config/config.h"
#include "os/os.h"

constexpr inline bool SHOW_ICONS = true;

struct BytesToGiB {
  u64 value;
};

// 1024^3 (size of 1 GiB)
constexpr u64 GIB = 1'073'741'824;

template <>
struct std::formatter<BytesToGiB> : std::formatter<double> {
  auto format(const BytesToGiB& BTG, auto& ctx) const {
    return std::format_to(ctx.out(), "{:.2f}GiB", static_cast<f64>(BTG.value) / GIB);
  }
};

namespace ui {
  using ftxui::Color;

  constexpr int MAX_PARAGRAPH_LENGTH = 30;
  constexpr int PADDING              = 3;

  // Color themes
  struct Theme {
    Color::Palette16 icon;
    Color::Palette16 label;
    Color::Palette16 value;
    Color::Palette16 border;
    Color::Palette16 accent;
  };

  constexpr Theme DEFAULT_THEME = {
    .icon   = Color::Cyan,
    .label  = Color::Yellow,
    .value  = Color::White,
    .border = Color::GrayLight,
    .accent = Color::Magenta,
  };

  struct Icons {
    std::string_view user;
    std::string_view palette;
    std::string_view calendar;
    std::string_view host;
    std::string_view kernel;
    std::string_view os;
    std::string_view memory;
    std::string_view weather;
    std::string_view music;
    std::string_view disk;
    std::string_view shell;
    std::string_view desktop;
    std::string_view window_manager;
  };

  constexpr Icons EMPTY_ICONS = {
    .user           = "",
    .palette        = "",
    .calendar       = "",
    .host           = "",
    .kernel         = "",
    .os             = "",
    .memory         = "",
    .weather        = "",
    .music          = "",
    .disk           = "",
    .shell          = "",
    .desktop        = "",
    .window_manager = "",
  };

  // Using your original icons
  constexpr Icons NERD_ICONS = {
    .user           = "   ",
    .palette        = "   ",
    .calendar       = "   ",
    .host           = " 󰌢  ",
    .kernel         = "   ",
    .os             = "   ",
    .memory         = "   ",
    .weather        = "   ",
    .music          = "   ",
    .disk           = " 󰋊  ",
    .shell          = "   ",
    .desktop        = " 󰇄  ",
    .window_manager = "   ",
  };
}

namespace {
  template <typename T, typename E, typename ValueFunc, typename ErrorFunc>
  auto expected_visit(const std::expected<T, E>& exp, ValueFunc value_func, ErrorFunc error_func) {
    if (exp.has_value())
      return value_func(*exp);

    return error_func(exp.error());
  }

  fn GetDate() -> string {
    using namespace std::chrono;

    const year_month_day ymd = year_month_day { floor<days>(system_clock::now()) };

    string month = std::format("{:%B}", ymd);

    i32 day = static_cast<unsigned>(ymd.day());

    const char* suffix = (day >= 11 && day <= 13) ? "th"
                         : (day % 10 == 1)        ? "st"
                         : (day % 10 == 2)        ? "nd"
                         : (day % 10 == 3)        ? "rd"
                                                  : "th";

    return std::format("{} {}{}", month, day, suffix);
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
      data.date           = std::move(GetDate());
      data.host           = std::move(GetHost());
      data.kernel_version = std::move(GetKernelVersion());
      data.os_version     = std::move(GetOSVersion());
      data.mem_info       = std::move(GetMemInfo());

      // Desktop environment info
      data.desktop_environment = std::move(GetDesktopEnvironment());
      data.window_manager      = std::move(GetWindowManager());

      // Parallel execution for disk/shell only
      auto diskShell = std::async(std::launch::async, [] {
        auto [used, total] = GetDiskUsage();
        return std::make_tuple(used, total, GetShell());
      });

      // Conditional tasks
      std::future<WeatherOutput>                          weather;
      std::future<std::expected<string, NowPlayingError>> nowPlaying;

      if (config.weather.enabled)
        weather = std::async(std::launch::async, [&config] { return config.weather.getWeatherInfo(); });

      if (config.now_playing.enabled)
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
    return hbox(
      std::views::iota(0, 16) | std::views::transform([](int colorIndex) {
        return hbox({ text("◯") | bold | color(static_cast<Color::Palette256>(colorIndex)), text(" ") });
      }) |
      std::ranges::to<Elements>()
    );
  }

  fn SystemInfoBox(const Config& config, const SystemData& data) -> Element {
    // Fetch data
    const string& name              = config.general.name;
    const Weather weather           = config.weather;
    const bool    nowPlayingEnabled = config.now_playing.enabled;

    const auto& [userIcon, paletteIcon, calendarIcon, hostIcon, kernelIcon, osIcon, memoryIcon, weatherIcon, musicIcon, diskIcon, shellIcon, deIcon, wmIcon] =
      SHOW_ICONS ? ui::NERD_ICONS : ui::EMPTY_ICONS;

    Elements content;

    content.push_back(text(string(userIcon) + "Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    content.push_back(hbox(
      {
        text(string(paletteIcon)) | color(ui::DEFAULT_THEME.icon),
        CreateColorCircles(),
      }
    ));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    // Helper function for aligned rows
    auto createRow = [&]<typename Icon, typename Label, typename Value>(Icon&& icon, Label&& label, Value&& value)
      requires std::constructible_from<string, Icon> && std::constructible_from<string, Label> &&
               std::constructible_from<string, Value>
    {
      return hbox(
        {
          text(string(std::forward<Icon>(icon))) | color(ui::DEFAULT_THEME.icon),
          text(string(std::forward<Label>(label))) | color(ui::DEFAULT_THEME.label),
          filler(),
          text(string(std::forward<Value>(value))) | color(ui::DEFAULT_THEME.value),
          text(" "),
        }
      );
    };

    // System info rows
    content.push_back(createRow(calendarIcon, "Date", data.date));

    // Weather row
    if (weather.enabled && data.weather_info.has_value()) {
      const WeatherOutput& weatherInfo = data.weather_info.value();

      if (weather.show_town_name)
        content.push_back(hbox(
          {
            text(string(weatherIcon)) | color(ui::DEFAULT_THEME.icon),
            text("Weather") | color(ui::DEFAULT_THEME.label),
            filler(),

            hbox(
              {
                text(std::format("{}°F ", std::lround(weatherInfo.main.temp))),
                text("in "),
                text(weatherInfo.name),
                text(" "),
              }
            ) |
              color(ui::DEFAULT_THEME.value),
          }
        ));
      else
        content.push_back(hbox(
          {
            text(string(weatherIcon)) | color(ui::DEFAULT_THEME.icon),
            text("Weather") | color(ui::DEFAULT_THEME.label),
            filler(),

            hbox(
              {
                text(std::format("{}°F, {}", std::lround(weatherInfo.main.temp), weatherInfo.weather[0].description)),
                text(" "),
              }
            ) |
              color(ui::DEFAULT_THEME.value),
          }
        ));
    }

    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (!data.host.empty())
      content.push_back(createRow(hostIcon, "Host", data.host));

    if (!data.kernel_version.empty())
      content.push_back(createRow(kernelIcon, "Kernel", data.kernel_version));

    expected_visit(
      data.os_version,
      [&](const string& version) { content.push_back(createRow(string(osIcon), "OS", version)); },
      [](const string& error) { ERROR_LOG("Failed to get OS version: {}", error); }
    );

    expected_visit(
      data.mem_info,
      [&](const u64& mem) { content.push_back(createRow(memoryIcon, "RAM", std::format("{}", BytesToGiB { mem }))); },
      [](const string& error) { ERROR_LOG("Failed to get memory info: {}", error); }
    );

    // Add Disk usage row
    content.push_back(
      createRow(diskIcon, "Disk", std::format("{}/{}", BytesToGiB { data.disk_used }, BytesToGiB { data.disk_total }))
    );

    content.push_back(createRow(shellIcon, "Shell", data.shell));

    content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (data.desktop_environment.has_value() && *data.desktop_environment != data.window_manager)
      content.push_back(createRow(deIcon, "DE", *data.desktop_environment));

    if (!data.window_manager.empty())
      content.push_back(createRow(wmIcon, "WM", data.window_manager));

    // Now Playing row
    if (nowPlayingEnabled && data.now_playing.has_value()) {
      if (const std::expected<string, NowPlayingError>& nowPlayingResult = *data.now_playing;
          nowPlayingResult.has_value()) {
        const std::string& npText = *nowPlayingResult;

        content.push_back(separator() | color(ui::DEFAULT_THEME.border));
        content.push_back(hbox(
          {
            text(string(musicIcon)) | color(ui::DEFAULT_THEME.icon),
            text("Playing") | color(ui::DEFAULT_THEME.label),
            text(" "),
            filler(),
            paragraph(npText) | color(Color::Magenta) | size(WIDTH, LESS_THAN, 30),
            text(" "),
          }
        ));
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
