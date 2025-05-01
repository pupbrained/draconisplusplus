#pragma once

#ifdef __linux__

// clang-format off
#include <wayland-client.h> // Wayland client library

#include "src/util/defs.hpp"
#include "src/util/types.hpp"
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
    display* m_display;

   public:
    /**
     * Opens a Wayland display connection
     */
    DisplayGuard() : m_display(connect(nullptr)) {}
    ~DisplayGuard() {
      if (m_display)
        disconnect(m_display);
    }

    // Non-copyable
    DisplayGuard(const DisplayGuard&)                = delete;
    fn operator=(const DisplayGuard&)->DisplayGuard& = delete;

    // Movable
    DisplayGuard(DisplayGuard&& other) noexcept : m_display(std::exchange(other.m_display, nullptr)) {}
    fn operator=(DisplayGuard&& other) noexcept -> DisplayGuard& {
      if (this != &other) {
        if (m_display)
          disconnect(m_display);

        m_display = std::exchange(other.m_display, nullptr);
      }

      return *this;
    }

    [[nodiscard]] explicit operator bool() const { return m_display != nullptr; }

    [[nodiscard]] fn get() const -> display* { return m_display; }
    [[nodiscard]] fn fd() const -> util::types::i32 { return get_fd(m_display); }
  };
} // namespace wl

#endif // __linux__
