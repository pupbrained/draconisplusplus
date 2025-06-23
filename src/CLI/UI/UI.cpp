#include "UI.hpp"

#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace ftxui;
using namespace draconis::utils::types;

namespace draconis::ui {
  using config::Config;
  using core::system::SystemInfo;
  using services::weather::Report;

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
    .cpu                = "",
    .gpu                = "",
    .uptime             = "",
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
#if DRAC_ARCH_64BIT
    .cpu = " 󰻠  ", // 64-bit CPU
#else
    .cpu = " 󰻟  ", // 32-bit CPU
#endif
    .gpu    = "   ",
    .uptime = "   ",
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
    .cpu                = " 💻 ",
    .gpu                = " 🎨 ",
    .uptime             = " ⏰ ",
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
    constexpr Array<Pair<StringView, StringView>, 13> distro_icons {{
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
#endif // __linux__

    fn CreateColorCircles() -> Element {
      static const Element COLOR_CIRCLES = hbox({
        text("◯") | bold | color(static_cast<Color::Palette256>(0)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(1)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(2)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(3)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(4)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(5)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(6)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(7)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(8)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(9)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(10)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(11)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(12)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(13)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(14)),
        text(" "),
        text("◯") | bold | color(static_cast<Color::Palette256>(15)),
        text(" "),
      });

      return COLOR_CIRCLES;
    }

    fn get_visual_width(const String& str) -> usize {
      return string_width(str);
    }

    fn get_visual_width_sv(const StringView& sview) -> usize {
      return string_width(String(sview));
    }

    fn find_max_label_len(const std::vector<RowInfo>& rows) -> usize {
      usize maxWidth = 0;
      for (const RowInfo& row : rows) maxWidth = std::max(maxWidth, get_visual_width_sv(row.label));

      return maxWidth;
    }
  } // namespace

#if DRAC_ENABLE_WEATHER
  fn CreateUI(const Config& config, const SystemInfo& data, Option<Report> weather) -> Element {
#else
  fn CreateUI(const Config& config, const SystemInfo& data) -> Element {
#endif
    const String& name = config.general.name;

    // clang-format off
    const auto& [
      calendarIcon,
      deIcon,
      diskIcon,
      hostIcon,
      kernelIcon,
      memoryIcon,
      cpuIcon,
      gpuIcon,
      uptimeIcon,
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

    Vec<RowInfo> initialRows;    // Date, Weather
    Vec<RowInfo> systemInfoRows; // Host, OS, Kernel
    Vec<RowInfo> hardwareRows;   // RAM, Disk, CPU, GPU
    Vec<RowInfo> softwareRows;   // Shell, Packages
    Vec<RowInfo> envInfoRows;    // DE, WM

    if (data.date)
      initialRows.push_back({ .icon = calendarIcon, .label = "Date", .value = *data.date });

#if DRAC_ENABLE_WEATHER
    if (weather) {
      const auto& [temperature, townName, description] = *weather;

      CStr tempUnit = config.weather.units == services::weather::Unit::METRIC ? "C" : "F";

      initialRows.push_back(
        {
          .icon  = weatherIcon,
          .label = "Weather",
          .value = config.weather.showTownName && townName
            ? std::format("{}°{} in {}", std::lround(temperature), tempUnit, *townName)
            : std::format("{}°{}, {}", std::lround(temperature), tempUnit, description),
        }
      );
    }
#endif // DRAC_ENABLE_WEATHER

    if (data.host && !data.host->empty())
      systemInfoRows.push_back({ .icon = hostIcon, .label = "Host", .value = *data.host });

    if (data.osVersion)
      systemInfoRows.push_back({
#ifdef __linux__
        .icon = GetDistroIcon(*data.osVersion).value_or(osIcon),
#else
        .icon = osIcon,
#endif
        .label = "OS",
        .value = *data.osVersion,
      });

    if (data.kernelVersion)
      systemInfoRows.push_back({ .icon = kernelIcon, .label = "Kernel", .value = *data.kernelVersion });

    if (data.memInfo)
      hardwareRows.push_back(
        {
          .icon  = memoryIcon,
          .label = "RAM",
          .value = std::format("{}/{}", BytesToGiB(data.memInfo->usedBytes), BytesToGiB(data.memInfo->totalBytes)),
        }
      );

    if (data.diskUsage)
      hardwareRows.push_back(
        {
          .icon  = diskIcon,
          .label = "Disk",
          .value = std::format("{}/{}", BytesToGiB(data.diskUsage->usedBytes), BytesToGiB(data.diskUsage->totalBytes)),
        }
      );

    if (data.cpuModel)
      hardwareRows.push_back({ .icon = cpuIcon, .label = "CPU", .value = *data.cpuModel });

    if (data.gpuModel)
      hardwareRows.push_back({ .icon = gpuIcon, .label = "GPU", .value = *data.gpuModel });

    if (data.uptime)
      hardwareRows.push_back(
        { .icon = uptimeIcon, .label = "Uptime", .value = std::format("{}", SecondsToFormattedDuration { *data.uptime }) }
      );

    if (data.shell)
      softwareRows.push_back({ .icon = shellIcon, .label = "Shell", .value = *data.shell });

#if DRAC_ENABLE_PACKAGECOUNT
    if (data.packageCount && *data.packageCount > 0)
      softwareRows.push_back({ .icon = packageIcon, .label = "Packages", .value = std::format("{}", *data.packageCount) });
#endif

    bool addedDe = false;

    if (data.desktopEnv && (!data.windowMgr || *data.desktopEnv != *data.windowMgr)) {
      envInfoRows.push_back({ .icon = deIcon, .label = "DE", .value = *data.desktopEnv });
      addedDe = true;
    }

    if (data.windowMgr && (!addedDe || (data.desktopEnv && *data.desktopEnv != *data.windowMgr)))
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

    const usize maxLabelWidthInitial  = find_max_label_len(initialRows);
    const usize maxLabelWidthSystem   = find_max_label_len(systemInfoRows);
    const usize maxLabelWidthHardware = find_max_label_len(hardwareRows);
    const usize maxLabelWidthSoftware = find_max_label_len(softwareRows);
    const usize maxLabelWidthEnv      = find_max_label_len(envInfoRows);

    const usize requiredWidthInitialW  = iconActualWidth + maxLabelWidthInitial;
    const usize requiredWidthSystemW   = iconActualWidth + maxLabelWidthSystem;
    const usize requiredWidthHardwareW = iconActualWidth + maxLabelWidthHardware;
    const usize requiredWidthSoftwareW = iconActualWidth + maxLabelWidthSoftware;
    const usize requiredWidthEnvW      = iconActualWidth + maxLabelWidthEnv;

    fn calculateRowVisualWidth = [&](const RowInfo& row, const usize requiredLabelVisualWidth) -> usize {
      return requiredLabelVisualWidth + get_visual_width(row.value) + get_visual_width_sv(" ");
    };

    for (const RowInfo& row : initialRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthInitialW));

    for (const RowInfo& row : systemInfoRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthSystemW));

    for (const RowInfo& row : hardwareRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthHardwareW));

    for (const RowInfo& row : softwareRows)
      maxContentWidth = std::max(maxContentWidth, calculateRowVisualWidth(row, requiredWidthSoftwareW));

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
#endif // DRAC_ENABLE_NOWPLAYING

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

    content.push_back(text(String(userIcon) + "Hello " + name + "! ") | bold | color(DEFAULT_THEME.icon));
    content.push_back(separator() | color(DEFAULT_THEME.border));
    content.push_back(hbox({ text(String(paletteIcon)) | color(DEFAULT_THEME.icon), CreateColorCircles() }));

    const bool section1Present = !initialRows.empty();
    const bool section2Present = !systemInfoRows.empty();
    const bool section3Present = !hardwareRows.empty();
    const bool section4Present = !softwareRows.empty();
    const bool section5Present = !envInfoRows.empty();

    if (section1Present)
      content.push_back(separator() | color(DEFAULT_THEME.border));

    for (const RowInfo& row : initialRows) content.push_back(createStandardRow(row, requiredWidthInitialW));

    if ((section1Present && (section2Present || section3Present || section4Present || section5Present)) || (!section1Present && section2Present))
      content.push_back(separator() | color(DEFAULT_THEME.border));

    for (const RowInfo& row : systemInfoRows) content.push_back(createStandardRow(row, requiredWidthSystemW));

    if (section2Present && (section3Present || section4Present || section5Present))
      content.push_back(separator() | color(DEFAULT_THEME.border));

    for (const RowInfo& row : hardwareRows) content.push_back(createStandardRow(row, requiredWidthHardwareW));

    if (section3Present && (section4Present || section5Present))
      content.push_back(separator() | color(DEFAULT_THEME.border));

    for (const RowInfo& row : softwareRows) content.push_back(createStandardRow(row, requiredWidthSoftwareW));

    if (section4Present && section5Present)
      content.push_back(separator() | color(DEFAULT_THEME.border));

    for (const RowInfo& row : envInfoRows) content.push_back(createStandardRow(row, requiredWidthEnvW));

#if DRAC_ENABLE_NOWPLAYING
    if ((section1Present || section2Present || section3Present || section4Present || section5Present) && nowPlayingActive)
      content.push_back(separator() | color(DEFAULT_THEME.border));

    if (nowPlayingActive) {
      content.push_back(hbox({
        text(String(musicIcon)) | color(DEFAULT_THEME.icon),
        text("Playing") | color(DEFAULT_THEME.label),
        text(" "),
        filler(),
        paragraphAlignRight(npText) | color(Color::Magenta) | size(WIDTH, LESS_THAN, paragraphLimit),
        text(" "),
      }));
    }
#endif // DRAC_ENABLE_NOWPLAYING

    return hbox({ vbox(content) | borderRounded | color(Color::White), filler() });
  }
} // namespace draconis::ui
