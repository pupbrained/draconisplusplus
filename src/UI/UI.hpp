#pragma once

#include <ftxui/dom/elements.hpp> // ftxui::Element
#include <ftxui/screen/color.hpp> // ftxui::Color

#include "Core/SystemData.hpp"

#include "Config/Config.hpp"

#include "Util/Types.hpp"

namespace ui {
  struct Theme {
    ftxui::Color::Palette16 icon;
    ftxui::Color::Palette16 label;
    ftxui::Color::Palette16 value;
    ftxui::Color::Palette16 border;
  };

  extern const Theme DEFAULT_THEME;

  struct Icons {
    util::types::StringView user;
    util::types::StringView palette;
    util::types::StringView calendar;
    util::types::StringView host;
    util::types::StringView kernel;
    util::types::StringView os;
    util::types::StringView memory;
    util::types::StringView weather;
    util::types::StringView music;
    util::types::StringView disk;
    util::types::StringView shell;
    util::types::StringView package;
    util::types::StringView desktop;
    util::types::StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const os::SystemData& data) -> ftxui::Element;
} // namespace ui
