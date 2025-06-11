#pragma once

#include <ftxui/dom/elements.hpp> // ftxui::Element
#include <ftxui/screen/color.hpp> // ftxui::Color

#include "Config/Config.hpp"

#include "Core/System.hpp"

#include "Util/Types.hpp"

namespace ui {
  using ftxui::Color;
  using util::types::StringView;

  struct Theme {
    Color::Palette16 icon;
    Color::Palette16 label;
    Color::Palette16 value;
    Color::Palette16 border;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    StringView calendar;
    StringView desktopEnvironment;
    StringView disk;
    StringView host;
    StringView kernel;
    StringView memory;
    StringView cpu;
    StringView gpu;
#if DRAC_ENABLE_NOWPLAYING
    StringView music;
#endif
    StringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    StringView package;
#endif
    StringView palette;
    StringView shell;
    StringView user;
#if DRAC_ENABLE_WEATHER
    StringView weather;
#endif
    StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const os::System& data) -> ftxui::Element;
} // namespace ui
