#pragma once

#ifdef __linux__

// clang-format off
#include <wayland-client.h> // Wayland client library

#include "src/core/util/defs.hpp"
#include "src/core/util/types.hpp"
// clang-format on

struct wl_display;

namespace wl {
  using display = wl_display;

  // NOLINTBEGIN(readability-identifier-naming)
  inline fn connect(const char* name) -> display* { return wl_display_connect(name); }
  inline fn disconnect(display* display) -> void { wl_display_disconnect(display); }
  inline fn get_fd(display* display) -> int { return wl_display_get_fd(display); }
  // NOLINTEND(readability-identifier-naming)

  /**
   * RAII wrapper for Wayland display connections
   * Automatically handles resource acquisition and cleanup
   */
  class DisplayGuard {
    display* m_Display;

   public:
    /**
     * Opens a Wayland display connection
     */
    DisplayGuard() : m_Display(connect(nullptr)) {}
    ~DisplayGuard() {
      if (m_Display)
        disconnect(m_Display);
    }

    // Non-copyable
    DisplayGuard(const DisplayGuard&)                = delete;
    fn operator=(const DisplayGuard&)->DisplayGuard& = delete;

    // Movable
    DisplayGuard(DisplayGuard&& other) noexcept : m_Display(std::exchange(other.m_Display, nullptr)) {}
    fn operator=(DisplayGuard&& other) noexcept -> DisplayGuard& {
      if (this != &other) {
        if (m_Display)
          disconnect(m_Display);

        m_Display = std::exchange(other.m_Display, nullptr);
      }

      return *this;
    }

    [[nodiscard]] explicit operator bool() const { return m_Display != nullptr; }

    [[nodiscard]] fn get() const -> display* { return m_Display; }
    [[nodiscard]] fn fd() const -> util::types::i32 { return get_fd(m_Display); }
  };
} // namespace wl

#endif // __linux__
