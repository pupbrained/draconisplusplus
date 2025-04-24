#pragma once

#ifdef __linux__

#include <X11/Xlib.h>
#include <wayland-client.h>

#include "src/util/macros.h"

namespace os::linux {
  /**
   * RAII wrapper for X11 Display connections
   * Automatically handles resource acquisition and cleanup
   */
  class DisplayGuard {
    Display* m_Display;

   public:
    /**
     * Opens an X11 display connection
     * @param name Display name (nullptr for default)
     */
    explicit DisplayGuard(CStr name = nullptr);
    ~DisplayGuard();

    // Non-copyable
    DisplayGuard(const DisplayGuard&)                = delete;
    fn operator=(const DisplayGuard&)->DisplayGuard& = delete;

    // Movable
    DisplayGuard(DisplayGuard&& other) noexcept;
    fn operator=(DisplayGuard&& other) noexcept -> DisplayGuard&;

    [[nodiscard]] explicit operator bool() const;
    [[nodiscard]] fn       get() const -> Display*;
    [[nodiscard]] fn       defaultRootWindow() const -> Window;
  };

  /**
   * RAII wrapper for Wayland display connections
   * Automatically handles resource acquisition and cleanup
   */
  class WaylandDisplayGuard {
    wl_display* m_Display;

   public:
    /**
     * Opens a Wayland display connection
     */
    WaylandDisplayGuard();
    ~WaylandDisplayGuard();

    // Non-copyable
    WaylandDisplayGuard(const WaylandDisplayGuard&)                = delete;
    fn operator=(const WaylandDisplayGuard&)->WaylandDisplayGuard& = delete;

    // Movable
    WaylandDisplayGuard(WaylandDisplayGuard&& other) noexcept;
    fn operator=(WaylandDisplayGuard&& other) noexcept -> WaylandDisplayGuard&;

    [[nodiscard]] explicit operator bool() const;
    [[nodiscard]] fn       get() const -> wl_display*;
    [[nodiscard]] fn       fd() const -> i32;
  };
}

#endif
