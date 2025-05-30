#pragma once

#include <ftxui/dom/elements.hpp> // ftxui::Element
#include <ftxui/screen/color.hpp> // ftxui::Color

#include "Config/Config.hpp"

#include "Core/SystemData.hpp"

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
    StringView user;
    StringView palette;
    StringView calendar;
    StringView host;
    StringView kernel;
    StringView os;
    StringView memory;
    StringView weather;
    StringView music;
    StringView disk;
    StringView shell;
    StringView package;
    StringView desktopEnvironment;
    StringView windowManager;
  };

  extern const Icons ICON_TYPE;

  /**
   * @brief Creates the main UI element based on system data and configuration.
   * @param config The application configuration.
   * @param data The collected system data. @return The root ftxui::Element for rendering.
   */
  fn CreateUI(const Config& config, const os::SystemData& data) -> ftxui::Element;
} // namespace ui
