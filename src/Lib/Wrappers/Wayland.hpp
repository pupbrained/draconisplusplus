#pragma once

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

  #include <wayland-client.h> // Wayland client library

  #include <DracUtils/Definitions.hpp>
  #include <DracUtils/Logging.hpp>
  #include <DracUtils/Types.hpp>

namespace Wayland {
  using util::types::i32, util::types::CStr, util::types::None;

  using Display = wl_display;

  /**
   * @brief Connect to a Wayland display
   *
   * This function establishes a connection to a Wayland display. It takes a
   * display name as an argument.
   *
   * @param name The name of the display to connect to (or nullptr for default)
   * @return A pointer to the Wayland display object
   */
  inline fn Connect(CStr name) -> Display* {
    return wl_display_connect(name);
  }

  /**
   * @brief Disconnect from a Wayland display
   *
   * This function disconnects from a Wayland display.
   *
   * @param display The Wayland display object to disconnect from
   * @return void
   */
  inline fn Disconnect(Display* display) -> void {
    wl_display_disconnect(display);
  }

  /**
   * @brief Get the file descriptor for a Wayland display
   *
   * This function retrieves the file descriptor for a Wayland display.
   *
   * @param display The Wayland display object
   * @return The file descriptor for the Wayland display
   */
  inline fn GetFd(Display* display) -> i32 {
    return wl_display_get_fd(display);
  }

  /**
   * @brief RAII wrapper for Wayland display connections
   *
   * This class manages the connection to a Wayland display. It automatically
   * handles resource acquisition and cleanup.
   */
  class DisplayGuard {
    Display* m_display; ///< The Wayland display object

   public:
    /**
     * @brief Constructor
     *
     * This constructor sets up a custom logging handler for Wayland and
     * establishes a connection to the Wayland display.
     */
    DisplayGuard() {
      wl_log_set_handler_client([](CStr fmt, va_list args) -> void {
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

    /**
     * @brief Destructor
     *
     * This destructor disconnects from the Wayland display if it is valid.
     */
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

    /**
     * @brief Move assignment operator
     *
     * This operator transfers ownership of the Wayland display connection.
     *
     * @param other The other DisplayGuard object to move from
     * @return A reference to this object
     */
    fn operator=(DisplayGuard&& other) noexcept -> DisplayGuard& {
      if (this != &other) {
        if (m_display)
          Disconnect(m_display);

        m_display = std::exchange(other.m_display, nullptr);
      }

      return *this;
    }

    /**
     * @brief Check if the display guard is valid
     *
     * This function checks if the display guard is valid (i.e., if it holds a
     * valid Wayland display connection).
     *
     * @return True if the display guard is valid, false otherwise
     */
    [[nodiscard]] explicit operator bool() const {
      return m_display != nullptr;
    }

    /**
     * @brief Get the Wayland display connection
     *
     * This function retrieves the underlying Wayland display connection.
     *
     * @return The Wayland display connection
     */
    [[nodiscard]] fn get() const -> Display* {
      return m_display;
    }

    /**
     * @brief Get the file descriptor for the Wayland display
     *
     * This function retrieves the file descriptor for the Wayland display.
     *
     * @return The file descriptor for the Wayland display
     */
    [[nodiscard]] fn fd() const -> i32 {
      return GetFd(m_display);
    }
  };
} // namespace Wayland

#endif // __linux__ || __FreeBSD__ || __DragonFly__ || __NetBSD__
