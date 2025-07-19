#pragma once

#include <curl/curl.h>
#include <utility> // std::{exchange, move}

#include <Drac++/Utils/Error.hpp>
#include <Drac++/Utils/Types.hpp>

namespace Curl {
  namespace {
    using draconis::utils::error::DracError;
    using enum draconis::utils::error::DracErrorCode;

    using draconis::utils::types::Err;
    using draconis::utils::types::i32;
    using draconis::utils::types::i64;
    using draconis::utils::types::None;
    using draconis::utils::types::Option;
    using draconis::utils::types::RawPointer;
    using draconis::utils::types::Result;
    using draconis::utils::types::String;
    using draconis::utils::types::Unit;
    using draconis::utils::types::usize;
  } // namespace

  /**
   * @brief Options for initializing a Curl::Easy handle.
   */
  struct EasyOptions {
    Option<String> url                = None;    ///< URL to set for the transfer
    String*        writeBuffer        = nullptr; ///< Pointer to a string buffer to store the response
    Option<i64>    timeoutSecs        = None;    ///< Timeout for the entire request in seconds
    Option<i64>    connectTimeoutSecs = None;    ///< Timeout for the connection phase in seconds
    Option<String> userAgent          = None;    ///< User-agent string
  };

  /**
   * @brief RAII wrapper for CURL easy handle.
   */
  class Easy {
    CURL*             m_curl      = nullptr;
    Option<DracError> m_initError = None; ///< Stores any error that occurred during initialization via options constructor

    static fn writeCallback(RawPointer contents, const usize size, const usize nmemb, String* str) -> usize {
      const usize totalSize = size * nmemb;
      str->append(static_cast<char*>(contents), totalSize);
      return totalSize;
    }

   public:
    /**
     * @brief Default constructor. Initializes a CURL easy handle.
     */
    Easy()
      : m_curl(curl_easy_init()) {
      if (!m_curl)
        m_initError = DracError(ApiUnavailable, "curl_easy_init() failed");
    }

    /**
     * @brief Constructor with options. Initializes a CURL easy handle and sets options.
     * @param options The options to configure the CURL handle.
     */
    explicit Easy(const EasyOptions& options)
      : m_curl(curl_easy_init()) {
      if (!m_curl) {
        m_initError = DracError(ApiUnavailable, "curl_easy_init() failed");
        return;
      }

      if (options.url)
        if (Result res = setUrl(*options.url); !res) {
          m_initError = res.error();
          return;
        }

      if (options.writeBuffer)
        if (Result res = setWriteFunction(options.writeBuffer); !res) {
          m_initError = res.error();
          return;
        }

      if (options.timeoutSecs)
        if (Result res = setTimeout(*options.timeoutSecs); !res) {
          m_initError = res.error();
          return;
        }

      if (options.connectTimeoutSecs)
        if (Result res = setConnectTimeout(*options.connectTimeoutSecs); !res) {
          m_initError = res.error();
          return;
        }

      if (options.userAgent)
        if (Result res = setUserAgent(*options.userAgent); !res) {
          m_initError = res.error();
          return;
        }
    }

    /**
     * @brief Destructor. Cleans up the CURL easy handle.
     */
    ~Easy() {
      if (m_curl)
        curl_easy_cleanup(m_curl);
    }

    // Non-copyable
    Easy(const Easy&)                = delete;
    fn operator=(const Easy&)->Easy& = delete;

    /**
     * @brief Move constructor.
     * @param other The other Easy object to move from.
     */
    Easy(Easy&& other) noexcept
      : m_curl(std::exchange(other.m_curl, nullptr)), m_initError(std::move(other.m_initError)) {}

    /**
     * @brief Move assignment operator.
     * @param other The other Easy object to move from.
     * @return A reference to this object.
     */
    fn operator=(Easy&& other) noexcept -> Easy& {
      if (this != &other) {
        if (m_curl)
          curl_easy_cleanup(m_curl);
        m_curl      = std::exchange(other.m_curl, nullptr);
        m_initError = std::move(other.m_initError);
      }

      return *this;
    }

    /**
     * @brief Checks if the CURL handle is valid and initialized without errors.
     * @return True if the handle is valid and no initialization error occurred, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const {
      return m_curl != nullptr && !m_initError;
    }

    /**
     * @brief Gets any error that occurred during initialization via the options constructor.
     * @return An Option containing a DracError if initialization failed, otherwise None.
     */
    [[nodiscard]] fn getInitializationError() const -> const Option<DracError>& {
      return m_initError;
    }

    /**
     * @brief Gets the underlying CURL handle.
     * @return The CURL handle.
     */
    [[nodiscard]] fn get() const -> CURL* {
      return m_curl;
    }

    /**
     * @brief Sets a CURL option.
     * @tparam T The type of the option value.
     * @param option The CURL option to set.
     * @param value The value to set for the option.
     * @return A Result indicating success or failure.
     */
    template <typename T>
    fn setOpt(const CURLoption option, T value) -> Result<> {
      if (!m_curl)
        ERR(InternalError, "CURL handle is not initialized or init failed");

      if (m_initError)
        ERR(InternalError, "CURL handle initialization previously failed");

      if (const CURLcode res = curl_easy_setopt(m_curl, option, value); res != CURLE_OK)
        ERR_FMT(PlatformSpecific, "curl_easy_setopt failed: {}", curl_easy_strerror(res));

      return {};
    }

    /**
     * @brief Performs a blocking file transfer.
     * @return A Result indicating success or failure.
     */
    fn perform() -> Result<> {
      if (!m_curl)
        ERR(InternalError, "CURL handle is not initialized or init failed");

      if (m_initError)
        ERR_FMT(InternalError, "Cannot perform request, CURL handle initialization failed: {}", m_initError->message);

      if (const CURLcode res = curl_easy_perform(m_curl); res != CURLE_OK)
        ERR_FMT(ApiUnavailable, "curl_easy_perform failed: {}", curl_easy_strerror(res));

      return {};
    }

    /**
     * @brief Gets information from a CURL transfer.
     * @tparam T The type of the information to get.
     * @param info The CURLINFO to get.
     * @param value A pointer to store the retrieved information.
     * @return A Result indicating success or failure.
     */
    template <typename T>
    fn getInfo(const CURLINFO info, T* value) -> Result<> {
      if (!m_curl)
        ERR(InternalError, "CURL handle is not initialized or init failed");

      if (m_initError)
        ERR_FMT(InternalError, "CURL handle initialization previously failed: {}", m_initError->message);

      if (const CURLcode res = curl_easy_getinfo(m_curl, info, value); res != CURLE_OK)
        ERR_FMT(PlatformSpecific, "curl_easy_getinfo failed: {}", curl_easy_strerror(res));

      return {};
    }

    /**
     * @brief Escapes a URL string.
     * @param url The URL string to escape.
     * @return A Result containing the escaped string or an error.
     */
    static fn escape(const String& url) -> Result<String> {
      char* escapedUrl = curl_easy_escape(nullptr, url.c_str(), static_cast<int>(url.length()));

      if (!escapedUrl)
        ERR(OutOfMemory, "curl_easy_escape failed");

      String result(escapedUrl);

      curl_free(escapedUrl);

      return result;
    }

    /**
     * @brief Sets the URL for the transfer.
     * @param url The URL to set.
     * @return A Result indicating success or failure.
     */
    fn setUrl(const String& url) -> Result<> {
      return setOpt(CURLOPT_URL, url.c_str());
    }

    /**
     * @brief Sets the write function and data for the transfer.
     * @param buffer The string buffer to write the response to.
     * @return A Result indicating success or failure.
     */
    fn setWriteFunction(String* buffer) -> Result<> {
      if (!buffer)
        ERR(InvalidArgument, "Write buffer cannot be null");

      if (Result res = setOpt(CURLOPT_WRITEFUNCTION, writeCallback); !res)
        return res;

      return setOpt(CURLOPT_WRITEDATA, buffer);
    }

    /**
     * @brief Sets the timeout for the transfer.
     * @param timeout The timeout in seconds.
     * @return A Result indicating success or failure.
     */
    fn setTimeout(const i64 timeout) -> Result<> {
      return setOpt(CURLOPT_TIMEOUT, timeout);
    }

    /**
     * @brief Sets the connect timeout for the transfer.
     * @param timeout The connect timeout in seconds.
     * @return A Result indicating success or failure.
     */
    fn setConnectTimeout(const i64 timeout) -> Result<> {
      return setOpt(CURLOPT_CONNECTTIMEOUT, timeout);
    }

    /**
     * @brief Sets the user agent for the transfer.
     * @param userAgent The user agent string.
     * @return A Result indicating success or failure.
     */
    fn setUserAgent(const String& userAgent) -> Result<> {
      return setOpt(CURLOPT_USERAGENT, userAgent.c_str());
    }
  };

  /**
   * @brief RAII wrapper for CURL multi handle.
   */
  class Multi {
    CURLM*            m_multi     = nullptr;
    Option<DracError> m_initError = None;

   public:
    /**
     * @brief Constructor. Initializes a CURL multi handle.
     */
    Multi()
      : m_multi(curl_multi_init()) {
      if (!m_multi)
        m_initError = DracError(ApiUnavailable, "curl_multi_init() failed");
    }

    /**
     * @brief Destructor. Cleans up the CURL multi handle.
     */
    ~Multi() {
      if (m_multi)
        curl_multi_cleanup(m_multi);
    }

    // Non-copyable
    Multi(const Multi&)                = delete;
    fn operator=(const Multi&)->Multi& = delete;

    /**
     * @brief Move constructor.
     * @param other The other Multi object to move from.
     */
    Multi(Multi&& other) noexcept
      : m_multi(std::exchange(other.m_multi, nullptr)), m_initError(std::move(other.m_initError)) {}

    /**
     * @brief Move assignment operator.
     * @param other The other Multi object to move from.
     * @return A reference to this object.
     */
    fn operator=(Multi&& other) noexcept -> Multi& {
      if (this != &other) {
        if (m_multi)
          curl_multi_cleanup(m_multi);

        m_multi     = std::exchange(other.m_multi, nullptr);
        m_initError = std::move(other.m_initError);
      }

      return *this;
    }

    /**
     * @brief Checks if the CURL multi handle is valid and initialized without error.
     * @return True if the handle is valid and no init error, false otherwise.
     */
    [[nodiscard]] explicit operator bool() const {
      return m_multi != nullptr && !m_initError;
    }

    /**
     * @brief Gets any error that occurred during initialization.
     * @return An Option containing a DracError if initialization failed, otherwise None.
     */
    [[nodiscard]] fn getInitializationError() const -> const Option<DracError>& {
      return m_initError;
    }

    /**
     * @brief Gets the underlying CURLM handle.
     * @return The CURLM handle.
     */
    [[nodiscard]] fn get() const -> CURLM* {
      return m_multi;
    }

    /**
     * @brief Adds an easy handle to the multi handle.
     * @param easyHandle The Easy handle to add.
     * @return A Result indicating success or failure.
     */
    fn addHandle(const Easy& easyHandle) -> Result<> {
      if (m_initError)
        ERR_FMT(InternalError, "CURL multi handle initialization previously failed: {}", m_initError->message);

      if (!easyHandle.get())
        ERR(InvalidArgument, "Provided CURL easy handle is not valid");

      if (easyHandle.getInitializationError())
        ERR_FMT(InvalidArgument, "Provided CURL easy handle failed initialization: {}", easyHandle.getInitializationError()->message);

      if (const CURLMcode res = curl_multi_add_handle(m_multi, easyHandle.get()); res != CURLM_OK)
        ERR_FMT(PlatformSpecific, "curl_multi_add_handle failed: {}", curl_multi_strerror(res));

      return {};
    }

    /**
     * @brief Removes an easy handle from the multi handle.
     * @param easyHandle The Easy handle to remove.
     * @return A Result indicating success or failure.
     */
    fn removeHandle(const Easy& easyHandle) -> Result<> {
      if (!m_multi)
        ERR(InternalError, "CURL multi handle is not initialized or init failed");

      if (m_initError)
        ERR_FMT(InternalError, "CURL multi handle initialization previously failed: {}", m_initError->message);

      if (!easyHandle.get()) // It's okay to try to remove a null handle, curl_multi_remove_handle handles it.
        ERR(InvalidArgument, "Provided CURL easy handle is not valid (for removal check)");

      if (const CURLMcode res = curl_multi_remove_handle(m_multi, easyHandle.get()); res != CURLM_OK) // CURLM_BAD_EASY_HANDLE is a possible error if handle was not in multi stack
        ERR_FMT(PlatformSpecific, "curl_multi_remove_handle failed: {}", curl_multi_strerror(res));

      return {};
    }

    /**
     * @brief Performs transfers on the multi handle.
     * @param stillRunning A pointer to an integer that will be set to the number of still running transfers.
     * @return A Result indicating success or failure.
     */
    fn perform(i32* stillRunning) -> Result<> {
      if (!m_multi)
        ERR(InternalError, "CURL multi handle is not initialized or init failed");

      if (m_initError)
        ERR_FMT(InternalError, "CURL multi handle initialization previously failed: {}", m_initError->message);

      if (const CURLMcode res = curl_multi_perform(m_multi, stillRunning); res != CURLM_OK && res != CURLM_CALL_MULTI_PERFORM)
        ERR_FMT(PlatformSpecific, "curl_multi_perform failed: {}", curl_multi_strerror(res));

      return {};
    }

    /**
     * @brief Reads information about completed transfers.
     * @param msgsInQueue A pointer to an integer that will be set to the number of messages in the queue.
     * @return A Result containing a CURLMsg pointer or an error. The caller is responsible for checking the msg field of CURLMsg.
     */
    fn infoRead(i32* msgsInQueue) -> Result<CURLMsg*> {
      if (!m_multi)
        ERR(InternalError, "CURL multi handle is not initialized or init failed");

      if (m_initError)
        ERR_FMT(InternalError, "CURL multi handle initialization previously failed: {}", m_initError->message);

      CURLMsg* msg = curl_multi_info_read(m_multi, msgsInQueue);

      return msg;
    }

    /**
     * @brief Waits for activity on any of the multi handle's file descriptors using poll.
     * @param timeoutMs The maximum time to wait in milliseconds.
     * @param numfds A pointer to an integer that will be set to the number of file descriptors with activity. Can be nullptr.
     * @return A Result indicating success or failure.
     */
    fn poll(const i32 timeoutMs, i32* numfds) -> Result<> {
      if (!m_multi)
        ERR(InternalError, "CURL multi handle is not initialized or init failed");

      if (m_initError)
        ERR_FMT(InternalError, "CURL multi handle initialization previously failed: {}", m_initError->message);

      if (const CURLMcode res = curl_multi_poll(m_multi, nullptr, 0, timeoutMs, numfds); res != CURLM_OK)
        ERR_FMT(PlatformSpecific, "curl_multi_poll failed: {}", curl_multi_strerror(res));

      return {};
    }

    /**
     * @brief Waits for activity on any of the multi handle's file descriptors using select semantics.
     * @param timeoutMs The maximum time to wait in milliseconds.
     * @param numfds A pointer to an integer that will be set to the number of file descriptors with activity.
     * @return A Result indicating success or failure.
     * @note This function is an alternative to poll and might be needed if curl_multi_poll is not available or desired.
     *       It requires more setup with curl_multi_fdset. For simplicity, poll is preferred if available.
     *       This is a simplified version; a full fdset handling is more complex.
     */
    fn wait(const i32 timeoutMs, i32* numfds) -> Result<> {
      if (!m_multi)
        ERR(InternalError, "CURL multi handle is not initialized or init failed");

      if (m_initError)
        ERR_FMT(InternalError, "CURL multi handle initialization previously failed: {}", m_initError->message);

      if (const CURLMcode res = curl_multi_wait(m_multi, nullptr, 0, timeoutMs, numfds); res != CURLM_OK)
        ERR_FMT(PlatformSpecific, "curl_multi_wait failed: {}", curl_multi_strerror(res));

      return {};
    }
  };

  /**
   * @brief Initializes CURL globally. Should be called once at the start of the program.
   * @param flags CURL global init flags.
   * @return A Result indicating success or failure.
   */
  inline fn GlobalInit(const i32 flags = CURL_GLOBAL_ALL) -> Result<> {
    if (const CURLcode res = curl_global_init(flags); res != CURLE_OK)
      ERR_FMT(PlatformSpecific, "curl_global_init failed: {}", curl_easy_strerror(res));

    return {};
  }

  /**
   * @brief Cleans up CURL globally. Should be called once at the end of the program.
   */
  inline fn GlobalCleanup() -> Unit {
    curl_global_cleanup();
  }
} // namespace Curl
