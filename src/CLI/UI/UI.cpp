#include "UI.hpp"

#include <sstream>

#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/Types.hpp>

using namespace draconis::utils::types;
using namespace draconis::utils::logging;

namespace draconis::ui {
  using config::Config;

  using core::system::SystemInfo;

  using services::weather::Report;

  constexpr Theme DEFAULT_THEME = {
    .icon   = LogColor::Cyan,
    .label  = LogColor::Yellow,
    .value  = LogColor::White,
    .border = LogColor::Gray,
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
    .cpu = " 󰻠  ",
#else
    .cpu = " 󰻟  ",
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

  struct UIGroup {
    Vec<RowInfo> rows;
    Vec<usize>   iconWidths;
    Vec<usize>   labelWidths;
    Vec<usize>   valueWidths;
    // Cached coloured strings so we don't have to run Colorize() on every
    // render pass.
    Vec<String> colouredIcons;
    Vec<String> colouredLabels;
    Vec<String> colouredValues;
    usize       maxLabelWidth = 0;
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

    constexpr fn GetDistroIcon(StringView distro) -> Option<StringView> {
      for (const auto& [distroName, distroIcon] : distro_icons)
        if (distro.contains(distroName))
          return distroIcon;

      return None;
    }
#endif // __linux__

    constexpr Array<StringView, 16> COLOR_CIRCLES {
      "\033[38;5;0m◯\033[0m",
      "\033[38;5;1m◯\033[0m",
      "\033[38;5;2m◯\033[0m",
      "\033[38;5;3m◯\033[0m",
      "\033[38;5;4m◯\033[0m",
      "\033[38;5;5m◯\033[0m",
      "\033[38;5;6m◯\033[0m",
      "\033[38;5;7m◯\033[0m",
      "\033[38;5;8m◯\033[0m",
      "\033[38;5;9m◯\033[0m",
      "\033[38;5;10m◯\033[0m",
      "\033[38;5;11m◯\033[0m",
      "\033[38;5;12m◯\033[0m",
      "\033[38;5;13m◯\033[0m",
      "\033[38;5;14m◯\033[0m",
      "\033[38;5;15m◯\033[0m"
    };

    constexpr fn IsWideCharacter(char32_t codepoint) -> bool {
      return (codepoint >= 0x1100 && codepoint <= 0x115F) || // Hangul Jamo
        (codepoint >= 0x2329 && codepoint <= 0x232A) ||      // Angle brackets
        (codepoint >= 0x2E80 && codepoint <= 0x2EFF) ||      // CJK Radicals Supplement
        (codepoint >= 0x2F00 && codepoint <= 0x2FDF) ||      // Kangxi Radicals
        (codepoint >= 0x2FF0 && codepoint <= 0x2FFF) ||      // Ideographic Description Characters
        (codepoint >= 0x3000 && codepoint <= 0x303E) ||      // CJK Symbols and Punctuation
        (codepoint >= 0x3041 && codepoint <= 0x3096) ||      // Hiragana
        (codepoint >= 0x3099 && codepoint <= 0x30FF) ||      // Katakana
        (codepoint >= 0x3105 && codepoint <= 0x312F) ||      // Bopomofo
        (codepoint >= 0x3131 && codepoint <= 0x318E) ||      // Hangul Compatibility Jamo
        (codepoint >= 0x3190 && codepoint <= 0x31BF) ||      // Kanbun
        (codepoint >= 0x31C0 && codepoint <= 0x31EF) ||      // CJK Strokes
        (codepoint >= 0x31F0 && codepoint <= 0x31FF) ||      // Katakana Phonetic Extensions
        (codepoint >= 0x3200 && codepoint <= 0x32FF) ||      // Enclosed CJK Letters and Months
        (codepoint >= 0x3300 && codepoint <= 0x33FF) ||      // CJK Compatibility
        (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||      // CJK Unified Ideographs Extension A
        (codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||      // CJK Unified Ideographs
        (codepoint >= 0xA000 && codepoint <= 0xA48F) ||      // Yi Syllables
        (codepoint >= 0xA490 && codepoint <= 0xA4CF) ||      // Yi Radicals
        (codepoint >= 0xAC00 && codepoint <= 0xD7A3) ||      // Hangul Syllables
        (codepoint >= 0xF900 && codepoint <= 0xFAFF) ||      // CJK Compatibility Ideographs
        (codepoint >= 0xFE10 && codepoint <= 0xFE19) ||      // Vertical Forms
        (codepoint >= 0xFE30 && codepoint <= 0xFE6F) ||      // CJK Compatibility Forms
        (codepoint >= 0xFF00 && codepoint <= 0xFF60) ||      // Fullwidth Forms
        (codepoint >= 0xFFE0 && codepoint <= 0xFFE6) ||      // Fullwidth Forms
        (codepoint >= 0x20000 && codepoint <= 0x2FFFD) ||    // CJK Unified Ideographs Extension B, C, D, E
        (codepoint >= 0x30000 && codepoint <= 0x3FFFD);      // CJK Unified Ideographs Extension F
    }

    constexpr fn DecodeUTF8(const StringView& str, usize& pos) -> char32_t {
      if (pos >= str.length())
        return 0;

      const fn getByte = [&](usize index) -> u8 {
        return static_cast<u8>(str[index]);
      };

      const u8 first = getByte(pos++);

      if ((first & 0x80) == 0) // ASCII (0xxxxxxx)
        return first;

      if ((first & 0xE0) == 0xC0) {
        // 2-byte sequence (110xxxxx 10xxxxxx)
        if (pos >= str.length())
          return 0;

        const u8 second = getByte(pos++);

        return ((first & 0x1F) << 6) | (second & 0x3F);
      }

      if ((first & 0xF0) == 0xE0) {
        // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
        if (pos + 1 >= str.length())
          return 0;

        const u8 second = getByte(pos++);
        const u8 third  = getByte(pos++);

        return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
      }

      if ((first & 0xF8) == 0xF0) {
        // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
        if (pos + 2 >= str.length())
          return 0;

        const u8 second = getByte(pos++);
        const u8 third  = getByte(pos++);
        const u8 fourth = getByte(pos++);

        return ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6) | (fourth & 0x3F);
      }

      return 0; // Invalid UTF-8
    }

    constexpr fn GetVisualWidth(const StringView& str) -> usize {
      usize width    = 0;
      bool  inEscape = false;
      usize pos      = 0;

      while (pos < str.length()) {
        const char current = str[pos];

        if (inEscape) {
          inEscape = (current != 'm');
          pos++;
        } else if (current == '\033') {
          inEscape = true;
          pos++;
        } else {
          const char32_t codepoint = DecodeUTF8(str, pos);
          if (codepoint != 0) {
            width += IsWideCharacter(codepoint) ? 2 : 1;
          }
        }
      }

      return width;
    }

    constexpr fn CreateDistributedColorCircles(usize availableWidth) -> String {
      if (COLOR_CIRCLES.empty() || availableWidth == 0)
        return "";

      const usize circleWidth = GetVisualWidth(COLOR_CIRCLES.at(0));
      const usize numCircles  = COLOR_CIRCLES.size();

      // Always show all circles with at least 1 space between each
      const usize minSpacingPerGap  = 1;
      const usize totalMinSpacing   = (numCircles - 1) * minSpacingPerGap;
      const usize totalCirclesWidth = numCircles * circleWidth;
      const usize requiredWidth     = totalCirclesWidth + totalMinSpacing;
      const usize effectiveWidth    = std::max(availableWidth, requiredWidth);

      if (numCircles == 1) {
        const usize padding = effectiveWidth / 2;
        return String(padding, ' ') + String(COLOR_CIRCLES.at(0));
      }

      const usize totalSpacing   = effectiveWidth - totalCirclesWidth;
      const usize spacingBetween = totalSpacing / (numCircles - 1);

      String result;
      result.reserve(effectiveWidth);

      for (usize i = 0; i < numCircles; ++i) {
        if (i > 0)
          result.append(spacingBetween, ' ');

        result += String(COLOR_CIRCLES.at(i));
      }

      return result;
    }

    constexpr fn ProcessGroup(UIGroup& group) -> usize {
      if (group.rows.empty())
        return 0;

      group.iconWidths.reserve(group.rows.size());
      group.labelWidths.reserve(group.rows.size());
      group.valueWidths.reserve(group.rows.size());
      group.colouredIcons.reserve(group.rows.size());
      group.colouredLabels.reserve(group.rows.size());
      group.colouredValues.reserve(group.rows.size());

      // Track maximum width while we populate cached data.
      usize groupMaxWidth = 0;

      for (const RowInfo& row : group.rows) {
        const usize labelWidth = GetVisualWidth(row.label);
        group.maxLabelWidth    = std::max(group.maxLabelWidth, labelWidth);

        const usize iconW  = GetVisualWidth(row.icon);
        const usize valueW = GetVisualWidth(row.value);

        group.iconWidths.push_back(iconW);
        group.labelWidths.push_back(labelWidth);
        group.valueWidths.push_back(valueW);

        group.colouredIcons.push_back(Colorize(row.icon, DEFAULT_THEME.icon));
        group.colouredLabels.push_back(Colorize(row.label, DEFAULT_THEME.label));
        group.colouredValues.push_back(Colorize(row.value, DEFAULT_THEME.value));

        groupMaxWidth = std::max(groupMaxWidth, iconW + valueW); // label handled after loop
      }

      // Final adjustment: add label width once (max label + 1 space).
      groupMaxWidth += group.maxLabelWidth + 1;

      return groupMaxWidth;
    }

    constexpr fn RenderGroup(String& out, const UIGroup& group, const usize maxContentWidth, const String& hBorder, bool& hasRenderedContent) {
      if (group.rows.empty())
        return;

      if (hasRenderedContent) {
        out += "├";
        out += hBorder;
        out += "┤\n";
      }

      for (usize i = 0; i < group.rows.size(); ++i) {
        const usize leftWidth  = group.iconWidths[i] + group.maxLabelWidth;
        const usize rightWidth = group.valueWidths[i];
        const usize padding    = (maxContentWidth >= leftWidth + rightWidth)
             ? maxContentWidth - (leftWidth + rightWidth)
             : 0;

        out += "│";
        out += group.colouredIcons[i];
        out += group.colouredLabels[i];
        out.append(group.maxLabelWidth - group.labelWidths[i], ' ');
        out.append(padding, ' ');
        out += group.colouredValues[i];
        out += " │\n";
      }

      hasRenderedContent = true;
    }

    constexpr fn WordWrap(const StringView& text, const usize wrapWidth) -> Vec<String> {
      Vec<String> lines;

      if (wrapWidth == 0) {
        lines.emplace_back(text);
        return lines;
      }

      std::stringstream textStream((String(text)));
      String            word;
      String            currentLine;

      while (textStream >> word) {
        if (!currentLine.empty() && GetVisualWidth(currentLine) + GetVisualWidth(word) + 1 > wrapWidth) {
          lines.emplace_back(currentLine);
          currentLine.clear();
        }

        if (!currentLine.empty())
          currentLine += " ";

        currentLine += word;
      }

      if (!currentLine.empty())
        lines.emplace_back(currentLine);

      return lines;
    }
  } // namespace

#if DRAC_ENABLE_WEATHER
  fn CreateUI(const Config& config, const SystemInfo& data, Option<Report> weather) -> String {
#else
  fn CreateUI(const Config& config, const SystemInfo& data) -> String {
#endif
    const String& name     = config.general.getName();
    const Icons&  iconType = ICON_TYPE;

    UIGroup initialGroup;
    UIGroup systemInfoGroup;
    UIGroup hardwareGroup;
    UIGroup softwareGroup;
    UIGroup envInfoGroup;

    {
      if (data.date)
        initialGroup.rows.push_back({ .icon = iconType.calendar, .label = "Date", .value = *data.date });

#if DRAC_ENABLE_WEATHER
      if (weather) {
        const auto& [temperature, townName, description] = *weather;

        PCStr tempUnit =
          config.weather.units == services::weather::UnitSystem::METRIC
          ? "C"
          : "F";

        initialGroup.rows.push_back(
          {
            .icon  = iconType.weather,
            .label = "Weather",
            .value = config.weather.showTownName && townName
              ? std::format("{}°{} in {}", std::lround(temperature), tempUnit, *townName)
              : std::format("{}°{}, {}", std::lround(temperature), tempUnit, description),
          }
        );
      }
#endif
    }

    {
      if (data.host && !data.host->empty())
        systemInfoGroup.rows.push_back({ .icon = iconType.host, .label = "Host", .value = *data.host });

      if (data.osVersion)
        systemInfoGroup.rows.push_back({
#ifdef __linux__
          .icon = GetDistroIcon(*data.osVersion).value_or(iconType.os),
#else
          .icon = iconType.os,
#endif
          .label = "OS",
          .value = *data.osVersion,
        });

      if (data.kernelVersion)
        systemInfoGroup.rows.push_back({ .icon = iconType.kernel, .label = "Kernel", .value = *data.kernelVersion });
    }

    {
      if (data.memInfo)
        hardwareGroup.rows.push_back({ .icon = iconType.memory, .label = "RAM", .value = std::format("{}/{}", BytesToGiB(data.memInfo->usedBytes), BytesToGiB(data.memInfo->totalBytes)) });

      if (data.diskUsage)
        hardwareGroup.rows.push_back({ .icon = iconType.disk, .label = "Disk", .value = std::format("{}/{}", BytesToGiB(data.diskUsage->usedBytes), BytesToGiB(data.diskUsage->totalBytes)) });

      if (data.cpuModel)
        hardwareGroup.rows.push_back({ .icon = iconType.cpu, .label = "CPU", .value = *data.cpuModel });

      if (data.gpuModel)
        hardwareGroup.rows.push_back({ .icon = iconType.gpu, .label = "GPU", .value = *data.gpuModel });

      if (data.uptime)
        hardwareGroup.rows.push_back({ .icon = iconType.uptime, .label = "Uptime", .value = std::format("{}", SecondsToFormattedDuration { *data.uptime }) });
    }

    {
      if (data.shell)
        softwareGroup.rows.push_back({ .icon = iconType.shell, .label = "Shell", .value = *data.shell });

#if DRAC_ENABLE_PACKAGECOUNT
      if (data.packageCount && *data.packageCount > 0)
        softwareGroup.rows.push_back({ .icon = iconType.package, .label = "Packages", .value = std::format("{}", *data.packageCount) });
#endif
    }

    {
      const bool deExists = data.desktopEnv.has_value();
      const bool wmExists = data.windowMgr.has_value();

      if (deExists && wmExists) {
        if (*data.desktopEnv == *data.windowMgr)
          envInfoGroup.rows.push_back({ .icon = iconType.windowManager, .label = "WM", .value = *data.windowMgr });
        else {
          envInfoGroup.rows.push_back({ .icon = iconType.desktopEnvironment, .label = "DE", .value = *data.desktopEnv });
          envInfoGroup.rows.push_back({ .icon = iconType.windowManager, .label = "WM", .value = *data.windowMgr });
        }
      } else if (deExists)
        envInfoGroup.rows.push_back({ .icon = iconType.desktopEnvironment, .label = "DE", .value = *data.desktopEnv });
      else if (wmExists)
        envInfoGroup.rows.push_back({ .icon = iconType.windowManager, .label = "WM", .value = *data.windowMgr });
    }

    Vec<UIGroup*> groups = { &initialGroup, &systemInfoGroup, &hardwareGroup, &softwareGroup, &envInfoGroup };

    usize maxContentWidth = 0;

    for (UIGroup* group : groups) {
      if (group->rows.empty())
        continue;

      maxContentWidth = std::max(maxContentWidth, ProcessGroup(*group));
    }

    String greetingLine = std::format("{}Hello {}!", iconType.user, name);
    maxContentWidth     = std::max(maxContentWidth, GetVisualWidth(greetingLine));

    // Calculate width needed for color circles (including minimum spacing)
    const usize circleWidth       = GetVisualWidth(COLOR_CIRCLES[0]);
    const usize totalCirclesWidth = COLOR_CIRCLES.size() * circleWidth;
    const usize minSpacingPerGap  = 1;
    const usize totalMinSpacing   = (COLOR_CIRCLES.size() - 1) * minSpacingPerGap;
    const usize colorCirclesWidth = GetVisualWidth(iconType.palette) + totalCirclesWidth + totalMinSpacing;
    maxContentWidth               = std::max(maxContentWidth, colorCirclesWidth);

#if DRAC_ENABLE_NOWPLAYING
    bool   nowPlayingActive = false;
    String npText;

    if (config.nowPlaying.enabled && data.nowPlaying) {
      npText           = std::format("{} - {}", data.nowPlaying->artist.value_or("Unknown Artist"), data.nowPlaying->title.value_or("Unknown Title"));
      nowPlayingActive = true;
    }
#endif

    String out;

    usize estimatedLines = 4;
    for (const UIGroup* grp : groups)
      estimatedLines += grp->rows.empty() ? 0 : (grp->rows.size() + 1);
#if DRAC_ENABLE_NOWPLAYING
    if (nowPlayingActive)
      ++estimatedLines;
#endif

    out.reserve(estimatedLines * (maxContentWidth + 4));

    const usize innerWidth = maxContentWidth + 1;

    String hBorder;
    hBorder.reserve(innerWidth * 3);
    for (usize i = 0; i < innerWidth; ++i) hBorder += "─";

    const fn createLine = [&](const String& left, const String& right = "") {
      const usize leftWidth  = GetVisualWidth(left);
      const usize rightWidth = GetVisualWidth(right);
      const usize padding    = (maxContentWidth >= leftWidth + rightWidth) ? maxContentWidth - (leftWidth + rightWidth) : 0;

      out += "│";
      out += left;
      out.append(padding, ' ');
      out += right;
      out += " │\n";
    };

    const fn createLeftAlignedLine = [&](const String& content) { createLine(content, ""); };

    // Top border and greeting
    out += "╭";
    out += hBorder;
    out += "╮\n";

    createLeftAlignedLine(Colorize(greetingLine, DEFAULT_THEME.icon));

    // Palette line
    out += "├";
    out += hBorder;
    out += "┤\n";

    const String paletteIcon    = Colorize(iconType.palette, DEFAULT_THEME.icon);
    const usize  availableWidth = maxContentWidth - GetVisualWidth(paletteIcon);
    createLeftAlignedLine(paletteIcon + CreateDistributedColorCircles(availableWidth));

    bool hasRenderedContent = true;

    for (const UIGroup* group : groups)
      RenderGroup(out, *group, maxContentWidth, hBorder, hasRenderedContent);

#if DRAC_ENABLE_NOWPLAYING
    if (nowPlayingActive) {
      if (hasRenderedContent) {
        out += "├";
        out += hBorder;
        out += "┤\n";
      }

      const String leftPart      = Colorize(iconType.music, DEFAULT_THEME.icon) + Colorize("Playing", DEFAULT_THEME.label);
      const usize  leftPartWidth = GetVisualWidth(leftPart);

      const usize availableWidth = maxContentWidth - leftPartWidth;

      const Vec<String> wrappedLines = WordWrap(npText, availableWidth);

      if (!wrappedLines.empty()) {
        createLine(leftPart, Colorize(wrappedLines[0], LogColor::Magenta));

        const String indent(leftPartWidth, ' ');

        for (usize i = 1; i < wrappedLines.size(); ++i) {
          String rightPart      = Colorize(wrappedLines[i], LogColor::Magenta);
          usize  rightPartWidth = GetVisualWidth(rightPart);

          usize padding = (maxContentWidth > leftPartWidth + rightPartWidth)
            ? maxContentWidth - leftPartWidth - rightPartWidth
            : 0;

          String lineContent = indent;
          lineContent.append(padding, ' ');
          lineContent.append(rightPart);
          createLine(lineContent);
        }
      }
    }
#endif

    out += "╰";
    out += hBorder;
    out += "╯\n";
    return out;
  }
} // namespace draconis::ui
