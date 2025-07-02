#pragma once

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)

  #include <wayland-client.h> // Wayland client library

  #include <Drac++/Utils/Definitions.hpp>
  #include <Drac++/Utils/Logging.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace Wayland {
  namespace {
    using draconis::utils::types::i32;
    using draconis::utils::types::None;
    using draconis::utils::types::PCStr;
    using draconis::utils::types::RawPointer;
    using draconis::utils::types::StringView;
    using draconis::utils::types::u32;
    using draconis::utils::types::Unit;
  } // namespace

  using Display          = wl_display;
  using Registry         = wl_registry;
  using Output           = wl_output;
  using RegistryListener = wl_registry_listener;
  using OutputListener   = wl_output_listener;
  using Interface        = wl_interface;

  inline const Interface wl_output_interface = ::wl_output_interface;

  constexpr u32 OUTPUT_MODE_CURRENT = WL_OUTPUT_MODE_CURRENT;

  /**
   * @brief Connect to a Wayland display
   *
   * This function establishes a connection to a Wayland display. It takes a
   * display name as an argument.
   *
   * @param name The name of the display to connect to (or nullptr for default)
   * @return A pointer to the Wayland display object
   */
  inline fn Connect(PCStr name) -> Display* {
    return wl_display_connect(name);
  }

  /**
   * @brief Disconnect from a Wayland display
   *
   * This function disconnects from a Wayland display.
   *
   * @param display The Wayland display object to disconnect from
   * @return Unit
   */
  inline fn Disconnect(Display* display) -> Unit {
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
   * @brief Get the registry for a Wayland display
   *
   * @param display The Wayland display object
   * @return The registry for the Wayland display
   */
  inline fn GetRegistry(Display* display) -> Registry* {
    return wl_display_get_registry(display);
  }

  /**
   * @brief Add a listener to a Wayland registry
   *
   * @param registry The Wayland registry object
   * @param listener The listener to add
   * @param data The data to pass to the listener
   * @return 0 on success, -1 on failure
   */
  inline fn AddRegistryListener(Registry* registry, const RegistryListener* listener, RawPointer data) -> i32 {
    return wl_registry_add_listener(registry, listener, data);
  }

  /**
   * @brief Process Wayland events
   *
   * @param display The Wayland display object
   * @return The number of events dispatched
   */
  inline fn Roundtrip(Display* display) -> i32 {
    return wl_display_roundtrip(display);
  }

  /**
   * @brief Bind to a Wayland object
   *
   * @param registry The Wayland registry object
   * @param name The name of the object to bind to
   * @param interface The interface to bind to
   * @param version The version of the interface to bind to
   * @return A pointer to the bound object
   */
  inline fn BindRegistry(Registry* registry, const u32 name, const Interface* interface, const u32 version) -> RawPointer {
    return wl_registry_bind(registry, name, interface, version);
  }

  /**
   * @brief Add a listener to a Wayland output
   *
   * @param output The Wayland output object
   * @param listener The listener to add
   * @param data The data to pass to the listener
   * @return 0 on success, -1 on failure
   */
  inline fn AddOutputListener(Output* output, const OutputListener* listener, RawPointer data) -> i32 {
    return wl_output_add_listener(output, listener, data);
  }

  /**
   * @brief Destroy a Wayland output
   *
   * @param output The Wayland output object
   */
  inline fn DestroyOutput(Output* output) -> Unit {
    wl_output_destroy(output);
  }

  /**
   * @brief Destroy a Wayland registry
   *
   * @param registry The Wayland registry object
   */
  inline fn DestroyRegistry(Registry* registry) -> Unit {
    wl_registry_destroy(registry);
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
      wl_log_set_handler_client([](PCStr fmt, va_list args) -> Unit {
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

    /**
     * @brief Get the registry for the Wayland display
     *
     * @return The registry for the Wayland display
     */
    [[nodiscard]] fn registry() const -> Registry* {
      return GetRegistry(m_display);
    }

    /**
     * @brief Process Wayland events
     *
     * @return The number of events dispatched
     */
    fn roundtrip() const -> i32 {
      return Roundtrip(m_display);
    }
  };
} // namespace Wayland

#endif // __linux__ || __FreeBSD__ || __DragonFly__ || __NetBSD__
