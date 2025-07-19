#pragma once

#if DRAC_USE_WAYLAND

  #include <cstring>          // std::strcmp
  #include <wayland-client.h> // Wayland client library

  #include <Drac++/Utils/DataTypes.hpp>
  #include <Drac++/Utils/Logging.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace Wayland {
  namespace {
    using draconis::utils::types::CStr;
    using draconis::utils::types::DisplayInfo;
    using draconis::utils::types::f64;
    using draconis::utils::types::i32;
    using draconis::utils::types::PCStr;
    using draconis::utils::types::RawPointer;
    using draconis::utils::types::StringView;
    using draconis::utils::types::u32;
    using draconis::utils::types::Unit;
    using draconis::utils::types::usize;
    using draconis::utils::types::Vec;
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

        Vec<CStr> buffer(static_cast<usize>(size) + 1);

        i32 writeSize = std::vsnprintf(buffer.data(), buffer.size(), fmt, args);

        if (writeSize < 0 || writeSize >= static_cast<int>(buffer.size())) {
          error_log("Wayland: Internal log formatting error (vsnprintf write failed).");
          return;
        }

        StringView msgView(buffer.data(), static_cast<usize>(writeSize));

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
    [[nodiscard]] fn roundtrip() const -> i32 {
      return Roundtrip(m_display);
    }
  };

  class DisplayManager {
   public:
    /**
     * @brief Constructor
     *
     * @param display The Wayland display object
     */
    explicit DisplayManager(Display* display)
      : m_display(display) {}

    /**
     * @brief Get information about all displays
     *
     * @return A vector of DisplayInfo objects
     */
    [[nodiscard]] fn getOutputs() -> Vec<DisplayInfo> {
      m_callbackData     = {};
      Registry* registry = GetRegistry(m_display);
      if (!registry)
        return {};

      const static RegistryListener REGISTRY_LISTENER = {
        .global        = registryHandler,
        .global_remove = nullptr,
      };

      AddRegistryListener(registry, &REGISTRY_LISTENER, this);
      Roundtrip(m_display);
      DestroyRegistry(registry);

      Vec<DisplayInfo> displays;
      for (const auto& output : m_callbackData.outputs) {
        displays.emplace_back(
          output.id,
          DisplayInfo::Resolution { .width = output.width, .height = output.height },
          output.refreshRate / 1000.0,
          displays.empty()
        );
      }

      return displays;
    }

    /**
     * @brief Get information about the primary display
     *
     * @return A DisplayInfo object for the primary display
     */
    [[nodiscard]] fn getPrimary() -> DisplayInfo {
      m_primaryDisplayData = {};
      Registry* registry   = GetRegistry(m_display);
      if (!registry)
        return {};

      const static RegistryListener REGISTRY_LISTENER = {
        .global        = primaryRegistry,
        .global_remove = nullptr,
      };

      AddRegistryListener(registry, &REGISTRY_LISTENER, this);
      while (!m_primaryDisplayData.done) {
        if (Roundtrip(m_display) < 0)
          break;
      }
      DestroyRegistry(registry);
      return m_primaryDisplayData.display;
    }

   private:
    Display* m_display; ///< The Wayland display object

    /**
     * @brief Data for display callbacks
     */
    struct CallbackData {
      struct Inner {
        usize id;
        usize width;
        usize height;
        f64   refreshRate;
      };

      Vec<Inner> outputs;
    };

    CallbackData m_callbackData; ///< Data for all outputs

    /**
     * @brief Data for primary display callbacks
     */
    struct PrimaryDisplayData {
      Output*     output = nullptr;
      DisplayInfo display;
      bool        done = false;
    };

    PrimaryDisplayData m_primaryDisplayData; ///< Data for the primary output

    /**
     * @brief Wayland output mode callback
     *
     * @param flags The output mode flags
     * @param width The width of the output
     * @param height The height of the output
     * @param refresh The refresh rate of the output
     */
    fn outputMode(u32 flags, i32 width, i32 height, i32 refresh) -> Unit {
      if (!(flags & WL_OUTPUT_MODE_CURRENT))
        return;

      if (!m_callbackData.outputs.empty()) {
        CallbackData::Inner& currentOutput = m_callbackData.outputs.back();
        currentOutput.width                = width > 0 ? width : 0;
        currentOutput.height               = height > 0 ? height : 0;
        currentOutput.refreshRate          = refresh > 0 ? refresh : 0;
      }
    }

    /**
     * @brief Static Wayland output mode callback
     *
     * @param data The user data
     * @param output The Wayland output object
     * @param flags The output mode flags
     * @param width The width of the output
     * @param height The height of the output
     * @param refresh The refresh rate of the output
     */
    static fn outputMode(RawPointer data, wl_output* /*output*/, u32 flags, i32 width, i32 height, i32 refresh) -> Unit {
      static_cast<DisplayManager*>(data)->outputMode(flags, width, height, refresh);
    }

    /**
     * @brief Wayland registry handler
     *
     * @param registry The Wayland registry object
     * @param objectId The object ID
     * @param interface The interface name
     * @param version The interface version
     */
    fn registryHandler(Registry* registry, u32 objectId, PCStr interface, u32 version) -> Unit {
      if (std::strcmp(interface, "wl_output") != 0)
        return;

      auto* output = static_cast<Output*>(BindRegistry(
        registry,
        objectId,
        &wl_output_interface,
        std::min(version, 2U)
      ));

      if (!output)
        return;

      const static OutputListener OUTPUT_LISTENER = {
        .geometry    = nullptr,
        .mode        = outputMode,
        .done        = nullptr,
        .scale       = nullptr,
        .name        = nullptr,
        .description = nullptr
      };

      m_callbackData.outputs.push_back({ .id = objectId, .width = 0, .height = 0, .refreshRate = 0.0 });
      AddOutputListener(output, &OUTPUT_LISTENER, this);
    }

    /**
     * @brief Static Wayland registry handler
     *
     * @param data The user data
     * @param registry The Wayland registry object
     * @param name The name of the object
     * @param interface The interface name
     * @param version The interface version
     */
    static fn registryHandler(RawPointer data, wl_registry* registry, u32 name, PCStr interface, u32 version) -> Unit {
      static_cast<DisplayManager*>(data)->registryHandler(registry, name, interface, version);
    }

    /**
     * @brief Wayland primary display mode callback
     *
     * @param flags The output mode flags
     * @param width The width of the output
     * @param height The height of the output
     * @param refresh The refresh rate of the output
     */
    fn primaryMode(u32 flags, i32 width, i32 height, i32 refresh) -> Unit {
      if (!(flags & WL_OUTPUT_MODE_CURRENT) || m_primaryDisplayData.done)
        return;

      m_primaryDisplayData.display.resolution  = { .width = static_cast<usize>(width), .height = static_cast<usize>(height) };
      m_primaryDisplayData.display.refreshRate = refresh > 0 ? refresh / 1000 : 0;
    }

    /**
     * @brief Static Wayland primary display mode callback
     *
     * @param data The user data
     * @param output The Wayland output object
     * @param flags The output mode flags
     * @param width The width of the output
     * @param height The height of the output
     * @param refresh The refresh rate of the output
     */
    static fn primaryMode(RawPointer data, wl_output* /*output*/, u32 flags, i32 width, i32 height, i32 refresh) -> Unit {
      static_cast<DisplayManager*>(data)->primaryMode(flags, width, height, refresh);
    }

    /**
     * @brief Wayland primary display done callback
     */
    fn primaryDone() -> Unit {
      if (m_primaryDisplayData.display.resolution.width > 0)
        m_primaryDisplayData.done = true;
    }

    /**
     * @brief Static Wayland primary display done callback
     *
     * @param data The user data
     * @param wl_output The Wayland output object
     */
    static fn primaryDone(RawPointer data, wl_output* /*wl_output*/) -> Unit {
      static_cast<DisplayManager*>(data)->primaryDone();
    }

    static fn primaryScale(RawPointer /*data*/, wl_output* /*wl_output*/, i32 /*scale*/) -> Unit {}
    static fn primaryGeometry(void* /*data*/, struct wl_output* /*wl_output*/, int32_t /*x*/, int32_t /*y*/, int32_t /*physical_width*/, int32_t /*physical_height*/, int32_t /*subpixel*/, const char* /*make*/, const char* /*model*/, int32_t /*transform*/) -> Unit {}

    /**
     * @brief Wayland primary display registry handler
     *
     * @param registry The Wayland registry object
     * @param name The name of the object
     * @param interface The interface name
     * @param version The interface version
     */
    fn primaryRegistry(Registry* registry, u32 name, PCStr interface, u32 version) -> Unit {
      if (m_primaryDisplayData.output != nullptr || std::strcmp(interface, "wl_output") != 0)
        return;

      m_primaryDisplayData.display.id        = name;
      m_primaryDisplayData.display.isPrimary = true;

      m_primaryDisplayData.output = static_cast<Output*>(BindRegistry(
        registry,
        name,
        &wl_output_interface,
        std::min(version, 2U)
      ));

      if (!m_primaryDisplayData.output)
        return;

      const static OutputListener LISTENER = {
        .geometry    = primaryGeometry,
        .mode        = primaryMode,
        .done        = primaryDone,
        .scale       = primaryScale,
        .name        = nullptr,
        .description = nullptr
      };

      AddOutputListener(m_primaryDisplayData.output, &LISTENER, this);
    }

    /**
     * @brief Static Wayland primary display registry handler
     *
     * @param data The user data
     * @param registry The Wayland registry object
     * @param name The name of the object
     * @param interface The interface name
     * @param version The interface version
     */
    static fn primaryRegistry(RawPointer data, wl_registry* registry, u32 name, PCStr interface, u32 version) -> Unit {
      static_cast<DisplayManager*>(data)->primaryRegistry(registry, name, interface, version);
    }
  };
} // namespace Wayland

#endif // DRAC_USE_WAYLAND
