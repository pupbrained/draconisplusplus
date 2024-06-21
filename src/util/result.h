#pragma once

#include <stdexcept>
#include <utility>
#include <variant>

#include "macros.h"
#include "types.h"

/**
 * @class Error
 * @brief Represents an error with a message.
 *
 * This class is used to encapsulate error messages that can be returned from functions.
 */
class Error {
 public:
  /**
   * @brief Constructs an Error with a message.
   * @param message The error message.
   */
  explicit Error(string message) : m_Message(std::move(message)) {}

  /**
   * @brief Retrieves the error message.
   * @return A constant reference to the error message string.
   */
  [[nodiscard]] fn message() const -> const string& { return m_Message; }

 private:
  string m_Message; ///< The error message.
};

// Primary template for Result with a default type of void

/**
 * @class Result
 * @brief Represents a result that can either be a value or an error.
 *
 * This is the primary template for Result, which defaults to handling void results.
 */
template <typename T = void>
class Result;

// Specialization for Result<void>

/**
 * @class Result<void>
 * @brief Specialization of Result for handling void results.
 *
 * This class is used when a function either succeeds with no value or fails with an error.
 */
template <>
class Result<void> {
 public:
  /**
   * @brief Constructs a successful Result.
   */
  Result() : m_Result(std::monostate {}) {}

  /**
   * @brief Constructs an error Result.
   * @param error The error object.
   */
  Result(const Error& error) : m_Result(error) {}

  /**
   * @brief Constructs an error Result.
   * @param error An rvalue reference to the error object.
   */
  Result(Error&& error) : m_Result(std::move(error)) {}

  /**
   * @brief Checks if the Result is successful.
   * @return True if the Result is successful, otherwise false.
   */
  [[nodiscard]] fn isOk() const -> bool { return std::holds_alternative<std::monostate>(m_Result); }

  /**
   * @brief Checks if the Result contains an error.
   * @return True if the Result contains an error, otherwise false.
   */
  [[nodiscard]] fn isErr() const -> bool { return std::holds_alternative<Error>(m_Result); }

  /**
   * @brief Throws an exception if the Result contains an error.
   *
   * This function should be called only if the Result is successful.
   */
  void value() const {
    if (isErr()) {
      throw std::logic_error("Attempted to access value of an error Result");
    }
  }

  /**
   * @brief Retrieves the error object.
   * @return A constant reference to the Error object.
   * @throws std::logic_error if the Result is successful.
   */
  [[nodiscard]] fn error() const -> const Error& {
    if (isOk()) {
      throw std::logic_error("Attempted to access error of an ok Result");
    }
    return std::get<Error>(m_Result);
  }

 private:
  std::variant<std::monostate, Error>
    m_Result; ///< The underlying result, which can be either void or an Error.
};

// Primary template for Result

/**
 * @class Result
 * @brief Represents a result that can either be a value of type T or an error.
 *
 * This template class is used to handle results that can either be a successful value or an error.
 * @tparam T The type of the successful value.
 */
template <typename T>
class Result {
 public:
  /**
   * @brief Constructs a successful Result with a value.
   * @param value The value of the Result.
   */
  Result(const T& value) : m_Result(value) {}

  /**
   * @brief Constructs a successful Result with a value.
   * @param value An rvalue reference to the value.
   */
  Result(T&& value) : m_Result(std::move(value)) {}

  /**
   * @brief Constructs an error Result.
   * @param error The error object.
   */
  Result(const Error& error) : m_Result(error) {}

  /**
   * @brief Constructs an error Result.
   * @param error An rvalue reference to the error object.
   */
  Result(Error&& error) : m_Result(std::move(error)) {}

  /**
   * @brief Checks if the Result is successful.
   * @return True if the Result is successful, otherwise false.
   */
  [[nodiscard]] fn isOk() const -> bool { return std::holds_alternative<T>(m_Result); }

  /**
   * @brief Checks if the Result contains an error.
   * @return True if the Result contains an error, otherwise false.
   */
  [[nodiscard]] fn isErr() const -> bool { return std::holds_alternative<Error>(m_Result); }

  /**
   * @brief Retrieves the value.
   * @return A constant reference to the value.
   * @throws std::logic_error if the Result contains an error.
   */
  fn value() const -> const T& {
    if (isErr()) {
      throw std::logic_error("Attempted to access value of an error Result");
    }
    return std::get<T>(m_Result);
  }

  /**
   * @brief Retrieves the error object.
   * @return A constant reference to the Error object.
   * @throws std::logic_error if the Result is successful.
   */
  [[nodiscard]] fn error() const -> const Error& {
    if (isOk()) {
      throw std::logic_error("Attempted to access error of an ok Result");
    }
    return std::get<Error>(m_Result);
  }

  /**
   * @brief Retrieves the value or returns a default value.
   * @param defaultValue The default value to return if the Result contains an error.
   * @return The value if the Result is successful, otherwise the default value.
   */
  fn valueOr(const T& defaultValue) const -> T {
    return isOk() ? std::get<T>(m_Result) : defaultValue;
  }

 private:
  std::variant<T, Error>
    m_Result; ///< The underlying result, which can be either a value of type T or an Error.
};

/**
 * @brief Helper function to create a successful Result.
 *
 * This function deduces the type of the value and creates a successful Result.
 * @tparam T The type of the value.
 * @param value The value to be stored in the Result.
 * @return A Result object containing the value.
 */
template <typename T>
fn Ok(T&& value) {
  return Result<std::decay_t<T>>(std::forward<T>(value));
}

/**
 * @brief Helper function to create a successful Result<void>.
 *
 * This function creates a successful Result that does not contain a value.
 * @return A Result<void> object indicating success.
 */
inline fn Ok() -> Result<void> { return {}; }
