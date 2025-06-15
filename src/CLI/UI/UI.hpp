#pragma once

#include <ftxui/dom/elements.hpp> // ftxui::Element
#include <ftxui/screen/color.hpp> // ftxui::Color

#include <Drac++/Core/System.hpp>

#include <DracUtils/Types.hpp>

#include "Config/Config.hpp"

namespace ui {
  struct Theme {
    ftxui::Color::Palette16 icon;
    ftxui::Color::Palette16 label;
    ftxui::Color::Palette16 value;
    ftxui::Color::Palette16 border;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    util::types::SZStringView calendar;
    util::types::SZStringView desktopEnvironment;
    util::types::SZStringView disk;
    util::types::SZStringView host;
    util::types::SZStringView kernel;
    util::types::SZStringView memory;
    util::types::SZStringView cpu;
    util::types::SZStringView gpu;
#if DRAC_ENABLE_NOWPLAYING
    util::types::SZStringView music;
#endif
    util::types::SZStringView os;
#if DRAC_ENABLE_PACKAGECOUNT
    util::types::SZStringView package;
#endif
    util::types::SZStringView palette;
    util::types::SZStringView shell;
    util::types::SZStringView user;
#if DRAC_ENABLE_WEATHER
    util::types::SZStringView weather;
#endif
    util::types::SZStringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const os::System& data) -> ftxui::Element;
} // namespace ui
