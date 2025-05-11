#pragma once

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

// clang-format off
#include <wayland-client.h> // Wayland client library

#include "src/util/defs.hpp"
#include "src/util/logging.hpp"
#include "src/util/types.hpp"
// clang-format on

struct wl_display;

namespace wl {
  using display = wl_display;

  inline fn Connect(const char* name) -> display* {
    return wl_display_connect(name);
  }
  inline fn Disconnect(display* display) -> void {
    wl_display_disconnect(display);
  }
  inline fn GetFd(display* display) -> int {
    return wl_display_get_fd(display);
  }

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
    DisplayGuard() {
      wl_log_set_handler_client([](const char* fmt, va_list args) -> void {
        using util::types::i32, util::types::StringView;

        va_list argsCopy;
        va_copy(argsCopy, args);
        i32 size = std::vsnprintf(nullptr, 0, fmt, argsCopy);
        va_end(argsCopy);

        if (size < 0) {
          error_log("Wayland: Internal log formatting error (vsnprintf size check failed).");
          return;
        }

        std::vector<char> buffer(static_cast<size_t>(size) + 1);

        i32 writeSize = std::vsnprintf(buffer.data(), buffer.size(), fmt, args);

        if (writeSize < 0 || writeSize >= static_cast<int>(buffer.size())) {
          error_log("Wayland: Internal log formatting error (vsnprintf write failed).");
          return;
        }

        StringView msgView(buffer.data(), static_cast<size_t>(writeSize));

        if (!msgView.empty() && msgView.back() == '\n')
          msgView.remove_suffix(1);

        debug_log("Wayland {}", msgView);
      });

      // NOLINTNEXTLINE(cppcoreguidelines-prefer-member-initializer) - needs to come after wl_log_set_handler_client
      m_display = Connect(nullptr);
    }

    ~DisplayGuard() {
      if (m_display)
        Disconnect(m_display);
    }

    // Non-copyable
    DisplayGuard(const DisplayGuard&)                = delete;
    fn operator=(const DisplayGuard&)->DisplayGuard& = delete;

    // Movable
    DisplayGuard(DisplayGuard&& other) noexcept
      : m_display(std::exchange(other.m_display, nullptr)) {}
    fn operator=(DisplayGuard&& other) noexcept -> DisplayGuard& {
      if (this != &other) {
        if (m_display)
          Disconnect(m_display);

        m_display = std::exchange(other.m_display, nullptr);
      }

      return *this;
    }

    [[nodiscard]] explicit operator bool() const {
      return m_display != nullptr;
    }

    [[nodiscard]] fn get() const -> display* {
      return m_display;
    }
    [[nodiscard]] fn fd() const -> util::types::i32 {
      return GetFd(m_display);
    }
  };
} // namespace wl

#endif // __linux__ || __FreeBSD__ || __DragonFly__ || __NetBSD__
