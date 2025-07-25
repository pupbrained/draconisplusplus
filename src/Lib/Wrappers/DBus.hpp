#pragma once

#if (defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)) && DRAC_ENABLE_NOWPLAYING

  #include <cstring>
  #include <dbus/dbus.h> // DBus Library
  #include <type_traits> // std::is_convertible_v
  #include <utility>     // std::exchange, std::forward

  #include <Drac++/Utils/Error.hpp>
  #include <Drac++/Utils/Types.hpp>

namespace DBus {
  namespace {
    using enum draconis::utils::error::DracErrorCode;

    using draconis::utils::types::i32;
    using draconis::utils::types::None;
    using draconis::utils::types::Option;
    using draconis::utils::types::RawPointer;
    using draconis::utils::types::Result;
    using draconis::utils::types::String;
    using draconis::utils::types::Unit;
  } // namespace

  /**
   * @brief RAII wrapper for DBusError. Automatically initializes and frees the error.
   */
  class Error {
    DBusError m_err {};                ///< The D-Bus error object
    bool      m_isInitialized = false; ///< Flag indicating if the error is initialized

   public:
    /**
     * @brief Constructor
     *
     * Initializes the D-Bus error object.
     */
    Error()
      : m_isInitialized(true) {
      dbus_error_init(&m_err);
    }

    /**
     * @brief Destructor
     *
     * Frees the D-Bus error object if it was initialized.
     */
    ~Error() {
      if (m_isInitialized)
        dbus_error_free(&m_err);
    }

    // Non-copyable
    Error(const Error&)                = delete;
    fn operator=(const Error&)->Error& = delete;

    /**
     * @brief Move constructor
     *
     * Transfers ownership of the D-Bus error object.
     *
     * @param other The other Error object to move from
     */
    Error(Error&& other) noexcept
      : m_err(other.m_err), m_isInitialized(other.m_isInitialized) {
      other.m_isInitialized = false;
      dbus_error_init(&other.m_err);
    }

    /**
     * @brief Move assignment operator
     *
     * Transfers ownership of the D-Bus error object.
     *
     * @param other The other Error object to move from
     * @return A reference to this object
     */
    fn operator=(Error&& other) noexcept -> Error& {
      if (this != &other) {
        if (m_isInitialized)
          dbus_error_free(&m_err);

        m_err           = other.m_err;
        m_isInitialized = other.m_isInitialized;

        other.m_isInitialized = false;
        dbus_error_init(&other.m_err);
      }
      return *this;
    }

    /**
     * @brief Checks if the error is set.
     * @return True if the error is set and initialized, false otherwise.
     */
    [[nodiscard]] fn isSet() const -> bool {
      return m_isInitialized && dbus_error_is_set(&m_err);
    }

    /**
     * @brief Gets the error message.
     * @return The error message string, or "" if not set or not initialized.
     */
    [[nodiscard]] fn message() const -> const char* {
      return isSet() ? m_err.message : "";
    }

    /**
     * @brief Gets the error name.
     * @return The error name string (e.g., "org.freedesktop.DBus.Error.Failed"), or "" if not set or not initialized.
     */
    [[nodiscard]] fn name() const -> const char* {
      return isSet() ? m_err.name : "";
    }

    /**
     * @brief Gets a pointer to the underlying DBusError. Use with caution.
     * @return Pointer to the DBusError struct.
     */
    [[nodiscard]] fn get() -> DBusError* {
      return &m_err;
    }

    /**
     * @brief Gets a const pointer to the underlying DBusError.
     * @return Const pointer to the DBusError struct.
     */
    [[nodiscard]] fn get() const -> const DBusError* {
      return &m_err;
    }
  };

  /**
   * @brief RAII wrapper for DBusMessageIter. Automatically frees the iterator.
   *
   * This class provides a convenient way to manage the lifetime of a D-Bus message iterator.
   */
  class MessageIter {
    DBusMessageIter m_iter {};         ///< The D-Bus message iterator
    bool            m_isValid = false; ///< Flag indicating if the iterator is valid

    // Allows the Message class to access private members of this class.
    friend class Message;

    /**
     * @brief Constructor
     *
     * Initializes the D-Bus message iterator.
     *
     * @param iter The D-Bus message iterator to wrap
     * @param isValid Flag indicating if the iterator is valid
     */
    explicit MessageIter(const DBusMessageIter& iter, const bool isValid)
      : m_iter(iter), m_isValid(isValid) {}

    /**
     * @brief Destructor
     *
     * Frees the D-Bus message iterator if it was initialized.
     *
     * @param value Pointer to the value to be freed
     *
     * @note This function is unsafe and should not be called directly.
     */
    fn getBasic(RawPointer value) -> Unit {
      if (m_isValid)
        dbus_message_iter_get_basic(&m_iter, value);
    }

   public:
    // Non-copyable
    MessageIter(const MessageIter&)                = delete;
    fn operator=(const MessageIter&)->MessageIter& = delete;

    // Non-movable
    MessageIter(MessageIter&&)                = delete;
    fn operator=(MessageIter&&)->MessageIter& = delete;

    // Destructor
    ~MessageIter() = default;

    /**
     * @brief Checks if the iterator is validly initialized.
     */
    [[nodiscard]] fn isValid() const -> bool {
      return m_isValid;
    }

    /**
     * @brief Gets the D-Bus type code of the current argument.
     * @return The D-Bus type code, or DBUS_TYPE_INVALID otherwise.
     */
    [[nodiscard]] fn getArgType() -> i32 {
      return m_isValid ? dbus_message_iter_get_arg_type(&m_iter) : DBUS_TYPE_INVALID;
    }

    /**
     * @brief Gets the element type of the container pointed to by the iterator.
     * Only valid if the iterator points to an ARRAY or VARIANT.
     * @return The D-Bus type code of the elements, or DBUS_TYPE_INVALID otherwise.
     */
    [[nodiscard]] fn getElementType() -> i32 {
      return m_isValid ? dbus_message_iter_get_element_type(&m_iter) : DBUS_TYPE_INVALID;
    }

    /**
     * @brief Advances the iterator to the next argument.
     * @return True if successful (moved to a next element), false if at the end or iterator is invalid.
     */
    fn next() -> bool {
      return m_isValid && dbus_message_iter_next(&m_iter);
    }

    /**
     * @brief Recurses into a container-type argument (e.g., array, struct, variant).
     * @return A new MessageIterGuard for the sub-container. The returned iterator might be invalid
     * if the current element is not a container or the main iterator is invalid.
     */
    [[nodiscard]] fn recurse() -> MessageIter {
      if (!m_isValid)
        return MessageIter({}, false);

      DBusMessageIter subIter;
      dbus_message_iter_recurse(&m_iter, &subIter);

      return MessageIter(subIter, true);
    }

    /**
     * @brief Helper to safely get a string argument from the iterator.
     * @return An Option containing the string value if the current arg is a valid string, or None otherwise.
     */
    [[nodiscard]] fn getString() -> Option<String> {
      if (m_isValid && getArgType() == DBUS_TYPE_STRING) {
        const char* strPtr = nullptr;

        // ReSharper disable once CppRedundantCastExpression
        getBasic(static_cast<RawPointer>(&strPtr));

        if (strPtr)
          return String(strPtr);
      }

      return None;
    }
  };

  /**
   * @brief RAII wrapper for DBusMessage. Automatically unrefs.
   */
  class Message {
    DBusMessage* m_msg = nullptr; ///< The D-Bus message object

   public:
    /**
     * @brief Constructor
     *
     * Initializes the D-Bus message object.
     *
     * @param msg The D-Bus message object to wrap
     */
    explicit Message(DBusMessage* msg = nullptr)
      : m_msg(msg) {}

    /**
     * @brief Destructor
     *
     * Frees the D-Bus message object if it was initialized.
     */
    ~Message() {
      if (m_msg)
        dbus_message_unref(m_msg);
    }

    // Non-copyable
    Message(const Message&)                = delete;
    fn operator=(const Message&)->Message& = delete;

    /**
     * @brief Move constructor
     *
     * Transfers ownership of the D-Bus message object.
     *
     * @param other The other Message object to move from
     */
    Message(Message&& other) noexcept
      : m_msg(std::exchange(other.m_msg, nullptr)) {}

    /**
     * @brief Move assignment operator
     *
     * Transfers ownership of the D-Bus message object.
     *
     * @param other The other Message object to move from
     * @return A reference to this object
     */
    fn operator=(Message&& other) noexcept -> Message& {
      if (this != &other) {
        if (m_msg)
          dbus_message_unref(m_msg);
        m_msg = std::exchange(other.m_msg, nullptr);
      }
      return *this;
    }

    /**
     * @brief Gets the underlying DBusMessage pointer. Use with caution.
     * @return The raw DBusMessage pointer, or nullptr if not holding a message.
     */
    [[nodiscard]] fn get() const -> DBusMessage* {
      return m_msg;
    }

    /**
     * @brief Initializes a message iterator for reading arguments from this message.
     * @return A MessageIterGuard. Check iter.isValid() before use.
     */
    [[nodiscard]] fn iterInit() const -> MessageIter {
      if (!m_msg)
        return MessageIter({}, false);

      DBusMessageIter iter;
      const bool      isValid = dbus_message_iter_init(m_msg, &iter);
      return MessageIter(iter, isValid);
    }

    /**
     * @brief Appends arguments of basic types to the message.
     * @tparam Args Types of the arguments to append.
     * @param args The arguments to append.
     * @return True if all arguments were appended successfully, false otherwise (e.g., allocation error).
     */
    template <typename... Args>
    [[nodiscard]] fn appendArgs(Args&&... args) -> bool {
      if (!m_msg)
        return false;

      DBusMessageIter iter;
      dbus_message_iter_init_append(m_msg, &iter);

      bool success = true;
      ((success = success && appendArgInternal(iter, std::forward<Args>(args))), ...); // NOLINT
      return success;
    }

    /**
     * @brief Creates a new D-Bus method call message.
     * @param destination Service name (e.g., "org.freedesktop.Notifications"). Can be null.
     * @param path Object path (e.g., "/org/freedesktop/Notifications"). Must not be null.
     * @param interface Interface name (e.g., "org.freedesktop.Notifications"). Can be null.
     * @param method Method name (e.g., "Notify"). Must not be null.
     * @return Result containing a MessageGuard on success, or DracError on failure.
     */
    static fn newMethodCall(const char* destination, const char* path, const char* interface, const char* method)
      -> Result<Message> {
      DBusMessage* rawMsg = dbus_message_new_method_call(destination, path, interface, method);

      if (!rawMsg)
        ERR(OutOfMemory, "dbus_message_new_method_call failed (allocation failed?)");

      return Message(rawMsg);
    }

   private:
    /**
     * @brief Appends a single argument to the message.
     * @tparam T Type of the argument to append.
     * @param iter The D-Bus message iterator.
     * @param arg The argument to append.
     * @return True if the argument was appended successfully, false otherwise (e.g., allocation error).
     */
    template <typename T>
    fn appendArgInternal(DBusMessageIter& iter, T&& arg) -> bool {
      using DecayedT = std::decay_t<T>;

      if constexpr (std::is_convertible_v<DecayedT, const char*>) {
        const char* valuePtr = static_cast<const char*>(std::forward<T>(arg));
        return dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, static_cast<const RawPointer>(&valuePtr));
      } else {
        static_assert(!sizeof(T*), "Unsupported type passed to appendArgs");
        return false;
      }
    }
  };

  /**
   * @brief RAII wrapper for DBusConnection. Automatically unrefs the connection.
   *
   * This class provides a convenient way to manage the lifetime of a D-Bus connection.
   */
  class Connection {
    DBusConnection* m_conn = nullptr; ///< The D-Bus connection object

   public:
    /**
     * @brief Constructor
     *
     * Initializes the D-Bus connection object.
     *
     * @param conn The D-Bus connection object to wrap
     */
    explicit Connection(DBusConnection* conn = nullptr)
      : m_conn(conn) {}

    /**
     * @brief Destructor
     *
     * Frees the D-Bus connection object if it was initialized.
     */
    ~Connection() {
      if (m_conn)
        dbus_connection_unref(m_conn);
    }

    // Non-copyable
    Connection(const Connection&)                = delete;
    fn operator=(const Connection&)->Connection& = delete;

    /**
     * @brief Move constructor
     *
     * Transfers ownership of the D-Bus connection object.
     *
     * @param other The other Connection object to move from
     */
    Connection(Connection&& other) noexcept
      : m_conn(std::exchange(other.m_conn, nullptr)) {}

    /**
     * @brief Move assignment operator
     *
     * Transfers ownership of the D-Bus connection object.
     *
     * @param other The other Connection object to move from
     * @return A reference to this object
     */
    fn operator=(Connection&& other) noexcept -> Connection& {
      if (this != &other) {
        if (m_conn)
          dbus_connection_unref(m_conn);

        m_conn = std::exchange(other.m_conn, nullptr);
      }
      return *this;
    }

    /**
     * @brief Gets the underlying DBusConnection pointer. Use with caution.
     * @return The raw DBusConnection pointer, or nullptr if not holding a connection.
     */
    [[nodiscard]] fn get() const -> DBusConnection* {
      return m_conn;
    }

    /**
     * @brief Sends a message and waits for a reply, blocking execution.
     * @param message The D-Bus message guard to send.
     * @param timeout_milliseconds Timeout duration in milliseconds.
     * @return Result containing the reply MessageGuard on success, or DracError on failure.
     */
    [[nodiscard]] fn sendWithReplyAndBlock(const Message& message, const i32 timeout_milliseconds = 1000) const
      -> Result<Message> {
      if (!m_conn || !message.get())
        ERR(InvalidArgument, "Invalid connection or message provided to sendWithReplyAndBlock");

      Error        err;
      DBusMessage* rawReply =
        dbus_connection_send_with_reply_and_block(m_conn, message.get(), timeout_milliseconds, err.get());

      if (err.isSet()) {
        if (const char* errName = err.name()) {
          if (strcmp(errName, DBUS_ERROR_TIMEOUT) == 0 || strcmp(errName, DBUS_ERROR_NO_REPLY) == 0)
            ERR(Timeout, err.message());

          if (strcmp(errName, DBUS_ERROR_SERVICE_UNKNOWN) == 0)
            ERR(NotFound, err.message());

          if (strcmp(errName, DBUS_ERROR_ACCESS_DENIED) == 0)
            ERR(PermissionDenied, err.message());
        }

        ERR(PlatformSpecific, err.message());
      }

      if (!rawReply)
        ERR(ApiUnavailable, "dbus_connection_send_with_reply_and_block returned null without setting error (likely timeout or disconnected)");

      return Message(rawReply);
    }

    /**
     * @brief Connects to a D-Bus bus type (Session or System).
     * @param bus_type The type of bus (DBUS_BUS_SESSION or DBUS_BUS_SYSTEM).
     * @return Result containing a ConnectionGuard on success, or DracError on failure.
     */
    static fn busGet(const DBusBusType bus_type) -> Result<Connection> {
      Error           err;
      DBusConnection* rawConn = dbus_bus_get(bus_type, err.get());

      if (err.isSet())
        ERR(ApiUnavailable, err.message());

      if (!rawConn)
        ERR(ApiUnavailable, "dbus_bus_get returned null without setting error");

      return Connection(rawConn);
    }
  };
} // namespace DBus

#endif // (__linux__ || __FreeBSD__ || __DragonFly__ || __NetBSD__) && DRAC_ENABLE_NOWPLAYING
