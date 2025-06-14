#include "UI.hpp"

#include <ranges>

#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "ftxui/dom/elements.hpp"

namespace ui {
  using namespace ftxui;
  using namespace util::types;

  constexpr Theme DEFAULT_THEME = {
    .icon   = Color::Cyan,
    .label  = Color::Yellow,
    .value  = Color::White,
    .border = Color::GrayLight,
  };

  [[maybe_unused]] static constexpr Icons NONE = {
    .calendar           = "",
    .desktopEnvironment = "",
    .disk               = "",
    .host               = "",
    .kernel             = "",
    .memory             = "",
#if DRAC_ENABLE_NOWPLAYING
    .music = "",
#endif
    .os = "",
#if DRAC_ENABLE_PACKAGECOUNT
    .package = "",
#endif
    .palette = "",
    .shell   = "",
    .user    = "",
#if DRAC_ENABLE_WEATHER
    .weather = "",
#endif
    .windowManager = "",
  };

  [[maybe_unused]] static constexpr Icons NERD = {
    .calendar           = "   ",
    .desktopEnvironment = " 󰇄  ",
    .disk               = " 󰋊  ",
    .host               = " 󰌢  ",
    .kernel             = "   ",
    .memory             = "   ",
#if DRAC_ENABLE_NOWPLAYING
    .music = "   ",
#endif
#ifdef __linux__
    .os = " 󰌽  ",
#elifdef __APPLE__
    .os = "   ",
#elifdef _WIN32
    .os = "   ",
#elifdef __FreeBSD__
    .os = "   ",
#else
    .os = "   ",
#endif
#if DRAC_ENABLE_PACKAGECOUNT
    .package = " 󰏖  ",
#endif
    .palette = "   ",
    .shell   = "   ",
    .user    = "   ",
#if DRAC_ENABLE_WEATHER
    .weather = "   ",
#endif
    .windowManager = "   ",
  };

  [[maybe_unused]] static constexpr Icons EMOJI = {
    .calendar           = " 📅 ",
    .desktopEnvironment = " 🖥️ ",
    .disk               = " 💾 ",
    .host               = " 💻 ",
    .kernel             = " 🫀 ",
    .memory             = " 🧠 ",
#if DRAC_ENABLE_NOWPLAYING
    .music = " 🎵 ",
#endif
    .os = " 🤖 ",
#if DRAC_ENABLE_PACKAGECOUNT
    .package = " 📦 ",
#endif
    .palette = " 🎨 ",
    .shell   = " 💲 ",
    .user    = " 👤 ",
#if DRAC_ENABLE_WEATHER
    .weather = " 🌈 ",
#endif
    .windowManager = " 🪟 ",
  };

  constexpr inline Icons ICON_TYPE = NERD;

  struct RowInfo {
    StringView icon;
    StringView label;
    String     value;
  };

  namespace {
#ifdef __linux__
    // clang-format off
    constexpr Array<Pair<String, String>, 13> distro_icons {{
      {        "NixOS", "   " },
      {        "Zorin", "   " },
      {       "Debian", "   " },
      {       "Fedora", "   " },
      {       "Gentoo", "   " },
      {       "Ubuntu", "   " },
      {      "Manjaro", "   " },
      {      "Pop!_OS", "   " },
      {   "Arch Linux", "   " },
      {   "Linux Mint", "   " },
      {   "Void Linux", "   " },
      { "Alpine Linux", "   " },
    }};
    // clang-format on

    fn GetDistroIcon(StringView distro) -> Option<StringView> {
      for (const auto& [distroName, distroIcon] : distro_icons)
        if (distro.contains(distroName))
          return distroIcon;

      return None;
    }
#endif

    fn CreateColorCircles() -> Element {
      auto colorView =
        std::views::iota(0, 16) | std::views::transform([](i32 colorIndex) {
          return ftxui::hbox({
            ftxui::text("◯") | ftxui::bold | ftxui::color(static_cast<ftxui::Color::Palette256>(colorIndex)),
            ftxui::text(" "),
          });
        });

      return hbox(Elements(std::ranges::begin(colorView), std::ranges::end(colorView)));
    }

    fn get_visual_width(const String& str) -> usize {
      return ftxui::string_width(str);
    }

    fn get_visual_width_sv(const StringView& sview) -> usize {
      return ftxui::string_width(String(sview));
    }

    fn find_max_label_len(const std::vector<RowInfo>& rows) -> usize {
      usize maxWidth = 0;
      for (const RowInfo& row : rows) maxWidth = std::max(maxWidth, get_visual_width_sv(row.label));

      return maxWidth;
    }
  } // namespace

  fn CreateUI(const Config& config, const os::System& data) -> Element {
    const String& name = config.general.name;

    // clang-format off
      const auto& [
        calendarIcon,
        deIcon,
        diskIcon,
        hostIcon,
        kernelIcon,
        memoryIcon,
#if DRAC_ENABLE_NOWPLAYING
        musicIcon,
#endif
        osIcon,
#if DRAC_ENABLE_PACKAGECOUNT
        packageIcon,
#endif
        paletteIcon,
        shellIcon,
        userIcon,
#if DRAC_ENABLE_WEATHER
        weatherIcon,
#endif
        wmIcon
      ] = ui::ICON_TYPE;
    // clang-format on

    std::vector<RowInfo> initialRows;    // Date, Weather
    std::vector<RowInfo> systemInfoRows; // Host, Kernel, OS, RAM, Disk, Shell, Packages
    std::vector<RowInfo> envInfoRows;    // DE, WM

    if (data.date)
      initialRows.push_back({ .icon = calendarIcon, .label = "Date", .value = *data.date });

#if DRAC_ENABLE_WEATHER
    if (config.weather.enabled && data.weather) {
      const weather::WeatherReport& weatherInfo = *data.weather;

      String weatherValue = config.weather.showTownName && weatherInfo.name
        ? std::format("{}°{} in {}", std::lround(weatherInfo.temperature), config.weather.units == config::WeatherUnit::METRIC ? "C" : "F", *weatherInfo.name)
        : std::format("{}°{}, {}", std::lround(weatherInfo.temperature), config.weather.units == config::WeatherUnit::METRIC ? "C" : "F", weatherInfo.description);

      initialRows.push_back({ .icon = weatherIcon, .label = "Weather", .value = std::move(weatherValue) });
    } else if (config.weather.enabled && !data.weather.has_value())
      error_at(data.weather.error());
#endif

    if (data.host && !data.host->empty())
      systemInfoRows.push_back({ .icon = hostIcon, .label = "Host", .value = *data.host });

    if (data.osVersion) {
      systemInfoRows.push_back({
#ifdef __linux__
        .icon = GetDistroIcon(*data.osVersion).value_or(osIcon),
#else
        .icon = osIcon,
#endif
        .label = "OS",
        .value = *data.osVersion,
      });
    }

    if (data.kernelVersion)
      systemInfoRows.push_back({ .icon = kernelIcon, .label = "Kernel", .value = *data.kernelVersion });

    if (data.memInfo)
      systemInfoRows.push_back({ .icon = memoryIcon, .label = "RAM", .value = std::format("{}/{}", BytesToGiB(data.memInfo->usedBytes), BytesToGiB(data.memInfo->totalBytes)) });
    else if (!data.memInfo.has_value())
      debug_at(data.memInfo.error());

    if (data.diskUsage)
      systemInfoRows.push_back(
        {
          .icon  = diskIcon,
          .label = "Disk",
          .value = std::format("{}/{}", BytesToGiB(data.diskUsage->usedBytes), BytesToGiB(data.diskUsage->totalBytes)),
        }
      );

    if (data.shell)
      systemInfoRows.push_back({ .icon = shellIcon, .label = "Shell", .value = *data.shell });

#if DRAC_ENABLE_PACKAGECOUNT
    if (data.packageCount) {
      if (*data.packageCount > 0)
        systemInfoRows.push_back({ .icon = packageIcon, .label = "Packages", .value = std::format("{}", *data.packageCount) });
    }
#endif

    bool addedDe = false;

    if (data.desktopEnv && (!data.windowMgr || *data.desktopEnv != *data.windowMgr)) {
      envInfoRows.push_back({ .icon = deIcon, .label = "DE", .value = *data.desktopEnv });
      addedDe = true;
    }

    if (data.windowMgr)
      if (!addedDe || (data.desktopEnv && *data.desktopEnv != *data.windowMgr))
        envInfoRows.push_back({ .icon = wmIcon, .label = "WM", .value = *data.windowMgr });

#if DRAC_ENABLE_NOWPLAYING
    bool   nowPlayingActive = false;
    String npText;

    if (config.nowPlaying.enabled && data.nowPlaying) {
      const String title  = data.nowPlaying->title.value_or("Unknown Title");
      const String artist = data.nowPlaying->artist.value_or("Unknown Artist");
      npText              = artist + " - " + title;
      nowPlayingActive    = true;
    }
#endif

    usize maxContentWidth = 0;

    const usize greetingWidth = get_visual_width_sv(userIcon) + get_visual_width_sv("Hello ") + get_visual_width(name) + get_visual_width_sv("! ");
    maxContentWidth           = std::max(maxContentWidth, greetingWidth);

    const usize paletteWidth = get_visual_width_sv(userIcon) + (16 * (get_visual_width_sv("◯") + get_visual_width_sv(" ")));
    maxContentWidth          = std::max(maxContentWidth, paletteWidth);

    const usize iconActualWidth = get_visual_width_sv(userIcon);

    const usize maxLabelWidthInitial = find_max_label_len(initialRows);
    const usize maxLabelWidthSystem  = find_max_label_len(systemInfoRows);
    const usize maxLabelWidthEnv     = find_max_label_len(envInfoRows);

    const usize requiredWidthInitialW = iconActualWidth + maxLabelWidthInitial;
    const usize requiredWidthSystemW  = iconActualWidth + maxLabelWidthSystem;
    const usize requiredWidthEnvW     = iconActualWidth + maxLabelWidthEnv;

    fn calculateRowVisualWidth = [&](const RowInfo& row, const usize requiredLabelVisualWidth) -> usize {
      return requiredLabelVisualWidth + get_visual_width(row.value) + get_visual_width_sv(" ");
    };

    for (const RowInfo& row : initialRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthInitialW));

    for (const RowInfo& row : systemInfoRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthSystemW));

    for (const RowInfo& row : envInfoRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthEnvW));

#if DRAC_ENABLE_NOWPLAYING
    const usize targetBoxWidth = maxContentWidth + 2;

    usize npFixedWidthLeft  = 0;
    usize npFixedWidthRight = 0;

    if (nowPlayingActive) {
      npFixedWidthLeft  = get_visual_width_sv(musicIcon) + get_visual_width_sv("Playing") + get_visual_width_sv(" ");
      npFixedWidthRight = get_visual_width_sv(" ");
    }

    i32 paragraphLimit = 1;

    if (nowPlayingActive) {
      i32 availableForParagraph = static_cast<i32>(targetBoxWidth) - static_cast<i32>(npFixedWidthLeft) - static_cast<i32>(npFixedWidthRight);

      availableForParagraph -= 2;

      paragraphLimit = std::max(1, availableForParagraph);
    }
#endif

    fn createStandardRow = [&](const RowInfo& row, const usize sectionRequiredVisualWidth) {
      return hbox({
        hbox({
          text(String(row.icon)) | color(ui::DEFAULT_THEME.icon),
          text(String(row.label)) | color(ui::DEFAULT_THEME.label),
        }) |
          size(WIDTH, EQUAL, static_cast<int>(sectionRequiredVisualWidth)),
        text(" "),
        filler(),
        text(row.value) | color(ui::DEFAULT_THEME.value),
        text(" "),
      });
    };

    Elements content;

    content.push_back(text(String(userIcon) + "Hello " + name + "! ") | bold | color(Color::Cyan));
    content.push_back(separator() | color(ui::DEFAULT_THEME.border));
    content.push_back(hbox({ text(String(paletteIcon)) | color(ui::DEFAULT_THEME.icon), CreateColorCircles() }));

    const bool section1Present = !initialRows.empty();
    const bool section2Present = !systemInfoRows.empty();
    const bool section3Present = !envInfoRows.empty();

    if (section1Present)
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    for (const RowInfo& row : initialRows) content.push_back(createStandardRow(row, requiredWidthInitialW));

    if ((section1Present && (section2Present || section3Present)) || (!section1Present && section2Present))
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    for (const RowInfo& row : systemInfoRows) content.push_back(createStandardRow(row, requiredWidthSystemW));

    if (section2Present && section3Present)
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    for (const RowInfo& row : envInfoRows) content.push_back(createStandardRow(row, requiredWidthEnvW));

#if DRAC_ENABLE_NOWPLAYING
    if ((section1Present || section2Present || section3Present) && nowPlayingActive)
      content.push_back(separator() | color(ui::DEFAULT_THEME.border));

    if (nowPlayingActive) {
      content.push_back(hbox({
        text(String(musicIcon)) | color(ui::DEFAULT_THEME.icon),
        text("Playing") | color(ui::DEFAULT_THEME.label),
        text(" "),
        filler(),
        paragraphAlignRight(npText) | color(Color::Magenta) | size(WIDTH, LESS_THAN, paragraphLimit),
        text(" "),
      }));
    }
#endif

    return hbox({ vbox(content) | borderRounded | color(Color::White), filler() });
  }
} // namespace ui
