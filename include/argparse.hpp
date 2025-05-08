/**
 * @file argparse.hpp
 * @brief Argument Parser for Modern C++
 * @author Pranav Srinivas Kumar <pranav.srinivas.kumar@gmail.com>
 * @copyright Copyright (c) 2019-2022 Pranav Srinivas Kumar and other contributors
 * @license MIT License <http://opensource.org/licenses/MIT>
 *
 * A powerful, flexible, and easy-to-use command-line argument parser for modern C++.
 * Provides a simple interface for defining, parsing, and validating command-line arguments.
 * Supports both positional and optional arguments, subcommands, and more.
 */

#pragma once

/*
 *   __ _ _ __ __ _ _ __   __ _ _ __ ___  ___
 *  / _` | '__/ _` | '_ \ / _` | '__/ __|/ _ \ Argument Parser for Modern C++
 * | (_| | | | (_| | |_) | (_| | |  \__ \  __/ http://github.com/p-ranav/argparse
 *  \__,_|_|  \__, | .__/ \__,_|_|  |___/\___|
 *            |___/|_|
 * * Licensed under the MIT License <http://opensource.org/licenses/MIT>.
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2019-2022 Pranav Srinivas Kumar <pranav.srinivas.kumar@gmail.com>
 * and other contributors.
 *
 * Permission is hereby  granted, free of charge, to any  person obtaining a copy
 * of this software and associated  documentation files (the "Software"), to deal
 * in the Software  without restriction, including without  limitation the rights
 * to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
 * copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
 * IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
 * FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
 * AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
 * LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <cassert>
#include <cerrno>

#ifndef ARGPARSE_MODULE_USE_STD_MODULE
  #include <algorithm>
  #include <array>
  #include <charconv>
  #include <cstdlib>
  #include <filesystem>
  #include <functional>
  #include <iostream>
  #include <iterator>
  #include <limits>
  #include <list>
  #include <map>
  #include <numeric>
  #include <optional>
  #include <ranges>
  #include <set>
  #include <sstream>
  #include <stdexcept>
  #include <string>
  #include <string_view>
  #include <tuple>
  #include <type_traits>
  #include <unordered_set>
  #include <utility>
  #include <variant>
  #include <vector>
#endif

#include "src/util/defs.hpp"
#include "src/util/error.hpp"
#include "src/util/types.hpp"

#ifndef ARGPARSE_CUSTOM_STRTOF
  #define ARGPARSE_CUSTOM_STRTOF strtof
#endif

#ifndef ARGPARSE_CUSTOM_STRTOD
  #define ARGPARSE_CUSTOM_STRTOD strtod
#endif

#ifndef ARGPARSE_CUSTOM_STRTOLD
  #define ARGPARSE_CUSTOM_STRTOLD strtold
#endif

// ReSharper disable CppTemplateParameterNeverUsed, CppDFATimeOver
// NOLINTBEGIN(readability-identifier-naming, readability-identifier-length, modernize-use-nullptr)
namespace argparse {
  using namespace util::types;
  using util::error::DracError, util::error::DracErrorCode;

  using ArgValue = std::variant<
    bool,
    int,
    double,
    String,
    std::filesystem::path,
    Vec<String>,
    Vec<int>,
    std::set<String>,
    std::set<int>>;

  namespace details {
    /**
     * @brief Trait to check if a type has container-like properties
     * @tparam T The type to check
     * @tparam void SFINAE parameter
     */
    template <typename T, typename = void>
    struct HasContainerTraits : std::false_type {};

    /**
     * @brief Specialization for std::string - not considered a container
     */
    template <>
    struct HasContainerTraits<String> : std::false_type {};

    /**
     * @brief Specialization for std::string_view - not considered a container
     */
    template <>
    struct HasContainerTraits<StringView> : std::false_type {};

    /**
     * @brief Specialization for types that have container-like properties
     * @tparam T The type to check
     */
    template <typename T>
    struct HasContainerTraits<T, std::void_t<typename T::value_type, decltype(std::declval<T>().begin()), decltype(std::declval<T>().end()), decltype(std::declval<T>().size())>> : std::true_type {};

    /**
     * @brief Convenience variable template for checking if a type is a container
     * @tparam T The type to check
     */
    template <typename T>
    inline constexpr bool IsContainer = HasContainerTraits<T>::value;

    /**
     * @brief Trait to check if a type can be streamed to std::ostream
     * @tparam T The type to check
     * @tparam void SFINAE parameter
     */
    template <typename T, typename = void>
    struct HasStreamableTraits : std::false_type {};

    /**
     * @brief Specialization for types that can be streamed to std::ostream
     * @tparam T The type to check
     */
    template <typename T>
    struct HasStreamableTraits<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>> : std::true_type {};

    /**
     * @brief Convenience variable template for checking if a type is streamable
     * @tparam T The type to check
     */
    template <typename T>
    inline constexpr bool IsStreamable = HasStreamableTraits<T>::value;

    /**
     * @brief Maximum number of elements to show when representing a container
     */
    constexpr usize repr_max_container_size = 5;

    /**
     * @brief Concept to check if a type can be formatted using std::format
     * @tparam T The type to check
     * @tparam CharT The character type for formatting
     */
    template <typename T, typename CharT = char>
    concept Formattable = requires(const T& t, std::basic_format_context<CharT*, CharT> ctx) {
      std::formatter<std::remove_cvref_t<T>, CharT>().format(t, ctx);
    };

    /**
     * @brief Convert a value to its string representation
     * @tparam T The type of the value to convert
     * @param val The value to convert
     * @return String representation of the value
     */
    template <typename T>
    static auto repr(const T& val) -> String {
      if constexpr (std::is_same_v<T, bool>)
        return val ? "true" : "false";
      else if constexpr (std::is_convertible_v<T, StringView>)
        return std::format("\"{}\"", String { StringView { val } });
      else if constexpr (IsContainer<T>) {
        String result = "{";

        const auto size = val.size();

        if (size > 0) {
          bool first = true;

          auto transformed_view = val | std::views::transform([](const auto& elem) { return details::repr(elem); });

          if (size <= repr_max_container_size) {
            for (const String& elem_repr : transformed_view) {
              if (!first)
                result += " ";

              result += elem_repr;
              first = false;
            }
          } else {
            for (const String& elem_repr : transformed_view | std::views::take(repr_max_container_size - 1)) {
              if (!first)
                result += " ";

              result += elem_repr;
              first = false;
            }

            result += "... ";

            result += details::repr(*std::prev(val.end()));
          }
        }

        result += "}";
        return result;
      } else if constexpr (Formattable<T>)
        return std::format("{}", val);
      else if constexpr (IsStreamable<T>) {
        std::stringstream out;
        out << val;
        return out.str();
      } else
        return "<not representable>";
    }

    /**
     * @brief Radix constants for number parsing
     */
    constexpr i32 radix_2  = 2;
    constexpr i32 radix_8  = 8;
    constexpr i32 radix_10 = 10;
    constexpr i32 radix_16 = 16;

    /**
     * @brief Helper function to apply a function with an additional argument
     * @tparam F Function type
     * @tparam Tuple Tuple type containing the base arguments
     * @tparam Extra Type of the additional argument
     * @tparam I... Index sequence
     * @param f Function to apply
     * @param t Tuple of base arguments
     * @param x Additional argument
     * @return Result of applying the function
     */
    template <class F, class Tuple, class Extra, usize... I>
    constexpr fn apply_plus_one_impl(F&& f, Tuple&& t, Extra&& x, std::index_sequence<I...> /*unused*/) -> decltype(auto) {
      return std::invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))..., std::forward<Extra>(x));
    }

    /**
     * @brief Wrapper for apply_plus_one_impl that creates the index sequence
     * @tparam F Function type
     * @tparam Tuple Tuple type containing the base arguments
     * @tparam Extra Type of the additional argument
     * @param f Function to apply
     * @param t Tuple of base arguments
     * @param x Additional argument
     * @return Result of applying the function
     */
    template <class F, class Tuple, class Extra>
    constexpr fn apply_plus_one(F&& f, Tuple&& t, Extra&& x) -> decltype(auto) {
      return details::apply_plus_one_impl(
        std::forward<F>(f), std::forward<Tuple>(t), std::forward<Extra>(x), std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<Tuple>>> {}
      );
    }

    /**
     * @brief Get a tuple of pointers to the start and end of a string view
     * @param s The string view to get pointers for
     * @return Tuple of (start pointer, end pointer)
     */
    constexpr fn pointer_range(const StringView s) noexcept -> std::tuple<const char*, const char*> {
      return { s.data(), s.data() + s.size() };
    }

    /**
     * @brief Check if a string view starts with a given prefix
     * @tparam CharT Character type
     * @tparam Traits Character traits type
     * @param prefix The prefix to check for
     * @param s The string to check
     * @return true if s starts with prefix, false otherwise
     */
    template <class CharT, class Traits>
    constexpr fn starts_with(std::basic_string_view<CharT, Traits> prefix, std::basic_string_view<CharT, Traits> s) noexcept -> bool {
      return s.substr(0, prefix.size()) == prefix;
    }

    /**
     * @brief Format flags for number parsing
     */
    enum class chars_format : u8 {
      scientific = 0xf1,              ///< Scientific notation (e.g., 1.23e4)
      fixed      = 0xf2,              ///< Fixed point notation (e.g., 123.45)
      hex        = 0xf4,              ///< Hexadecimal notation (e.g., 0x1a)
      binary     = 0xf8,              ///< Binary notation (e.g., 0b1010)
      general    = fixed | scientific ///< General format (either fixed or scientific)
    };

    /**
     * @brief Result of checking for binary prefix
     */
    struct ConsumeBinaryPrefixResult {
      bool       is_binary; ///< Whether the string had a binary prefix
      StringView rest;      ///< The string after removing the prefix
    };

    /**
     * @brief Check if a string starts with a binary prefix and remove it
     * @param s The string to check
     * @return Result containing whether a binary prefix was found and the remaining string
     */
    constexpr fn consume_binary_prefix(StringView s) -> ConsumeBinaryPrefixResult {
      if (starts_with(StringView { "0b" }, s) ||
          starts_with(StringView { "0B" }, s)) {
        s.remove_prefix(2);
        return { .is_binary = true, .rest = s };
      }

      return { .is_binary = false, .rest = s };
    }

    /**
     * @brief Result of checking for hexadecimal prefix
     */
    struct ConsumeHexPrefixResult {
      bool       is_hexadecimal; ///< Whether the string had a hex prefix
      StringView rest;           ///< The string after removing the prefix
    };

    using namespace std::literals;

    /**
     * @brief Check if a string starts with a hexadecimal prefix and remove it
     * @param s The string to check
     * @return Result containing whether a hex prefix was found and the remaining string
     */
    constexpr fn consume_hex_prefix(StringView s) -> ConsumeHexPrefixResult {
      if (starts_with("0x"sv, s) || starts_with("0X"sv, s)) {
        s.remove_prefix(2);
        return { .is_hexadecimal = true, .rest = s };
      }

      return { .is_hexadecimal = false, .rest = s };
    }

    /**
     * @brief Parse a string into a number using std::from_chars
     * @tparam T The type to parse into
     * @tparam Param The radix or format to use
     * @param s The string to parse
     * @return Result containing the parsed number or an error
     */
    template <class T, auto Param>
    fn do_from_chars(const StringView s) -> Result<T> {
      T x { 0 };
      auto [first, last] = pointer_range(s);
      auto [ptr, ec]     = std::from_chars(first, last, x, Param);

      if (ec == std::errc()) {
        if (ptr == last)
          return x;

        return Err(DracError(DracErrorCode::ParseError, std::format("pattern '{}' does not match to the end", String(s))));
      }

      if (ec == std::errc::invalid_argument)
        return Err(DracError(DracErrorCode::InvalidArgument, std::format("pattern '{}' not found", String(s))));

      if (ec == std::errc::result_out_of_range)
        return Err(DracError(DracErrorCode::ParseError, std::format("'{}' not representable", String(s))));

      return Err(DracError(DracErrorCode::InternalError, std::format("Unknown parsing error for '{}'", String(s))));
    }

    /**
     * @brief Functor for parsing numbers with a specific radix
     * @tparam T The type to parse into
     * @tparam Param The radix to use (defaults to 0 for automatic detection)
     */
    template <class T, auto Param = 0>
    struct parse_number {
      /**
       * @brief Parse a string into a number
       * @param s The string to parse
       * @return Result containing the parsed number or an error
       */
      static fn operator()(const StringView s)->Result<T> {
        return do_from_chars<T, Param>(s);
      }
    };

    /**
     * @brief Specialization for binary number parsing
     * @tparam T The type to parse into
     */
    template <class T>
    struct parse_number<T, radix_2> {
      /**
       * @brief Parse a binary string into a number
       * @param s The string to parse (must start with 0b or 0B)
       * @return Result containing the parsed number or an error
       */
      static fn operator()(const StringView s)->Result<T> {
        if (auto [ok, rest] = consume_binary_prefix(s); ok)
          return do_from_chars<T, radix_2>(rest);

        return Err(DracError(DracErrorCode::InvalidArgument, "pattern not found"));
      }
    };

    /**
     * @brief Specialization for hexadecimal number parsing
     * @tparam T The type to parse into
     */
    template <class T>
    struct parse_number<T, radix_16> {
      /**
       * @brief Parse a hexadecimal string into a number
       * @param s The string to parse (may start with 0x or 0X)
       * @return Result containing the parsed number or an error
       */
      static fn operator()(const StringView s)->Result<T> {
        Result<T> result;

        if (starts_with("0x"sv, s) || starts_with("0X"sv, s)) {
          if (auto [ok, rest] = consume_hex_prefix(s); ok)
            result = do_from_chars<T, radix_16>(rest);
          else
            return Err(DracError(DracErrorCode::InternalError, std::format("Inconsistent hex prefix detection for '{}'", String(s))));
        } else
          result = do_from_chars<T, radix_16>(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as hexadecimal: {}", String(s), result.error().message)));

        return result;
      }
    };

    /**
     * @brief Specialization for automatic number format detection
     * @tparam T The type to parse into
     */
    template <class T>
    struct parse_number<T> {
      /**
       * @brief Parse a string into a number, automatically detecting the format
       * @param s The string to parse
       * @return Result containing the parsed number or an error
       *
       * Supports:
       * - Hexadecimal (0x/0X prefix)
       * - Binary (0b/0B prefix)
       * - Octal (0 prefix)
       * - Decimal (no prefix)
       */
      static fn operator()(const StringView s)->Result<T> {
        if (auto [ok, rest] = consume_hex_prefix(s); ok) {
          Result<T> result = do_from_chars<T, radix_16>(rest);

          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as hexadecimal: {}", String(s), result.error().message)));

          return result;
        }

        if (auto [ok_binary, rest_binary] = consume_binary_prefix(s); ok_binary) {
          Result<T> result = do_from_chars<T, radix_2>(rest_binary);

          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as binary: {}", String(s), result.error().message)));

          return result;
        }

        if (starts_with("0"sv, s)) {
          Result<T> result = do_from_chars<T, radix_8>(s);

          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as octal: {}", String(s), result.error().message)));

          return result;
        }

        Result<T> result = do_from_chars<T, radix_10>(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as decimal integer: {}", String(s), result.error().message)));

        return result;
      }
    };

    /**
     * @brief Custom string to number conversion functions
     * @tparam T The type to convert to
     */
    template <class T>
    inline constexpr std::nullptr_t generic_strtod = nullptr;
    template <>
    inline const auto generic_strtod<float> = ARGPARSE_CUSTOM_STRTOF;
    template <>
    inline const auto generic_strtod<double> = ARGPARSE_CUSTOM_STRTOD;
    template <>
    inline const auto generic_strtod<long double> = ARGPARSE_CUSTOM_STRTOLD;

    /**
     * @brief Parse a string into a floating point number
     * @tparam T The floating point type to parse into
     * @param s The string to parse
     * @return Result containing the parsed number or an error
     */
    template <class T>
    fn do_strtod(const String& s) -> Result<T> {
      if (isspace(static_cast<unsigned char>(s[0])) || s[0] == '+')
        return Err(DracError(DracErrorCode::InvalidArgument, std::format("pattern '{}' not found", s)));

      auto [first, last] = pointer_range(s);

      char* ptr = nullptr;

      errno = 0;

      auto x = generic_strtod<T>(first, &ptr);

      if (errno == 0) {
        if (ptr == last)
          return x;

        return Err(DracError(DracErrorCode::ParseError, std::format("pattern '{}' does not match to the end", s)));
      }

      if (errno == ERANGE)
        return Err(DracError(DracErrorCode::ParseError, std::format("'{}' not representable", s)));

      return Err(DracError(std::error_code(errno, std::system_category())));
    }

    /**
     * @brief Specialization for general floating point format
     * @tparam T The floating point type to parse into
     */
    template <class T>
    struct parse_number<T, chars_format::general> {
      /**
       * @brief Parse a string into a floating point number in general format
       * @param s The string to parse
       * @return Result containing the parsed number or an error
       */
      fn operator()(const String& s)->Result<T> {
        if (auto [is_hex, rest] = consume_hex_prefix(s); is_hex)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::general does not parse hexfloat"));

        if (auto [is_bin, rest] = consume_binary_prefix(s); is_bin)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::general does not parse binfloat"));

        Result<T> result = do_strtod<T>(s);
        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as number: {}", s, result.error().message)));
        return result;
      }
    };

    /**
     * @brief Specialization for hexadecimal floating point format
     * @tparam T The floating point type to parse into
     */
    template <class T>
    struct parse_number<T, chars_format::hex> {
      /**
       * @brief Parse a string into a floating point number in hexadecimal format
       * @param s The string to parse (must start with 0x or 0X)
       * @return Result containing the parsed number or an error
       */
      fn operator()(const String& s)->Result<T> {
        if (auto [is_hex, rest] = consume_hex_prefix(s); !is_hex)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::hex requires hexfloat format (e.g., 0x1.2p3)"));

        if (auto [is_bin, rest] = consume_binary_prefix(s); is_bin)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::hex does not parse binfloat"));

        Result<T> result = do_strtod<T>(s);
        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as hexadecimal float: {}", s, result.error().message)));
        return result;
      }
    };

    /**
     * @brief Specialization for binary floating point format
     * @tparam T The floating point type to parse into
     */
    template <class T>
    struct parse_number<T, chars_format::binary> {
      /**
       * @brief Parse a string into a floating point number in binary format
       * @param s The string to parse (must start with 0b or 0B)
       * @return Result containing the parsed number or an error
       */
      fn operator()(const String& s)->Result<T> {
        if (auto [is_hex, rest] = consume_hex_prefix(s); is_hex)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::binary does not parse hexfloat"));

        if (auto [is_bin, rest] = consume_binary_prefix(s); !is_bin)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::binary requires binfloat format (e.g., 0b1.01p2)"));

        Result<T> result = do_strtod<T>(s);
        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as binary float: {}", s, result.error().message)));
        return result;
      }
    };

    /**
     * @brief Specialization for scientific floating point format
     * @tparam T The floating point type to parse into
     */
    template <class T>
    struct parse_number<T, chars_format::scientific> {
      /**
       * @brief Parse a string into a floating point number in scientific notation
       * @param s The string to parse (must contain e or E)
       * @return Result containing the parsed number or an error
       */
      fn operator()(const String& s)->Result<T> {
        if (const auto [is_hex, rest] = consume_hex_prefix(s); is_hex)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::scientific does not parse hexfloat"));

        if (const auto [is_bin, rest] = consume_binary_prefix(s); is_bin)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::scientific does not parse binfloat"));

        if (s.find_first_of("eE") == String::npos)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::scientific requires exponent part"));

        Result<T> result = do_strtod<T>(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as scientific notation: {}", s, result.error().message)));

        return result;
      }
    };

    /**
     * @brief Specialization for fixed point floating point format
     * @tparam T The floating point type to parse into
     */
    template <class T>
    struct parse_number<T, chars_format::fixed> {
      /**
       * @brief Parse a string into a floating point number in fixed point notation
       * @param s The string to parse (must not contain e or E)
       * @return Result containing the parsed number or an error
       */
      fn operator()(const String& s)->Result<T> {
        if (const auto [is_hex, rest] = consume_hex_prefix(s); is_hex)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::fixed does not parse hexfloat"));

        if (const auto [is_bin, rest] = consume_binary_prefix(s); is_bin)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::fixed does not parse binfloat"));

        if (s.find_first_of("eE") != String::npos)
          return Err(DracError(DracErrorCode::InvalidArgument, "chars_format::fixed does not parse exponent part"));

        Result<T> result = do_strtod<T>(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as fixed notation: {}", s, result.error().message)));

        return result;
      }
    };

    /**
     * @brief Concept to check if a type can be converted to a string
     * @tparam T The type to check
     */
    template <typename T>
    concept ToStringConvertible = std::convertible_to<T, std::string> ||
      std::convertible_to<T, std::string_view> ||
      requires(const T& t) { std::format("{}", t); };

    /**
     * @brief Join a range of strings with a separator
     * @tparam StrIt Iterator type for the string range
     * @param first Iterator to the first string
     * @param last Iterator past the last string
     * @param separator The separator to use between strings
     * @return The joined string
     */
    template <typename StrIt>
    fn join(StrIt first, StrIt last, const String& separator) -> String {
      if (first == last)
        return "";

      std::stringstream value;
      value << *first;
      ++first;

      while (first != last) {
        value << separator << *first;
        ++first;
      }

      return value.str();
    }

    /**
     * @brief Trait to check if a type can be converted using std::to_string
     * @tparam T The type to check
     */
    template <typename T>
    struct can_invoke_to_string {
      /**
       * @brief SFINAE test for std::to_string support
       * @tparam U The type to test
       */
      template <typename U>
      // ReSharper disable CppFunctionIsNotImplemented
      static fn test(int) -> decltype(std::to_string(std::declval<U>()), std::true_type {});

      /**
       * @brief Fallback for types without std::to_string support
       * @tparam U The type to test
       */
      template <typename U>
      static fn test(...) -> std::false_type;
      // ReSharper restore CppFunctionIsNotImplemented

      static constexpr bool value = decltype(test<T>(0))::value;
    };

    /**
     * @brief Trait to check if a type is supported for choice arguments
     * @tparam T The type to check
     */
    template <typename T>
    struct IsChoiceTypeSupported {
      using CleanType         = std::decay_t<T>;
      static const bool value = std::is_integral_v<CleanType> ||
        std::is_same_v<CleanType, String> ||
        std::is_same_v<CleanType, StringView> ||
        std::is_same_v<CleanType, const char*>;
    };

    /**
     * @brief Calculate the Levenshtein distance between two strings
     * @tparam StringType The string type to use
     * @param s1 First string
     * @param s2 Second string
     * @return The Levenshtein distance between s1 and s2
     */
    template <typename StringType>
    fn get_levenshtein_distance(const StringType& s1, const StringType& s2) -> usize {
      Vec<Vec<usize>> dp(
        s1.size() + 1, Vec<usize>(s2.size() + 1, 0)
      );

      for (usize i = 0; i <= s1.size(); ++i) {
        for (usize j = 0; j <= s2.size(); ++j) {
          if (i == 0) {
            dp[i][j] = j;
          } else if (j == 0) {
            dp[i][j] = i;
          } else if (s1[i - 1] == s2[j - 1]) {
            dp[i][j] = dp[i - 1][j - 1];
          } else {
            dp[i][j] = 1 + std::min<usize>({ dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1] });
          }
        }
      }

      return dp[s1.size()][s2.size()];
    }

    /**
     * @brief Find the most similar string in a map to a given input
     * @tparam MapType The map-like container type
     * @tparam ValueType The value type of the map
     * @param map The map to search in
     * @param input The input string to find matches for
     * @return The most similar string from the map
     */
    template <typename MapType>
    fn get_most_similar_string(const MapType& map, const String& input) -> String {
      String most_similar {};
      usize  min_distance = (std::numeric_limits<usize>::max)();

      for (const auto& entry : map)
        if (const usize distance = get_levenshtein_distance(entry.first, input); distance < min_distance) {
          min_distance = distance;
          most_similar = entry.first;
        }

      return most_similar;
    }

    /**
     * @brief Trait to check if a type is a specialization of a template
     * @tparam Test The type to check
     * @tparam Ref The template to check against
     */
    template <typename Test, template <typename...> class Ref>
    struct is_specialization : std::false_type {};

    /**
     * @brief Specialization for when Test is a specialization of Ref
     * @tparam Ref The template
     * @tparam Args The template arguments
     */
    template <template <typename...> class Ref, typename... Args>
    struct is_specialization<Ref<Args...>, Ref> : std::true_type {};

    /**
     * @brief Convenience variable template for checking template specialization
     * @tparam Test The type to check
     * @tparam Ref The template to check against
     */
    template <typename Test, template <typename...> class Ref>
    inline constexpr bool is_specialization_v = is_specialization<Test, Ref>::value;
  } // namespace details

  /**
   * @brief Enumeration for specifying the number of arguments pattern
   * @details Defines different patterns for how many arguments an option can accept
   */
  enum class nargs_pattern : u8 {
    optional,    ///< Argument is optional (0 or 1 arguments)
    any,         ///< Argument can accept any number of arguments (0 or more)
    at_least_one ///< Argument requires at least one argument (1 or more)
  };

  /**
   * @brief Enumeration for specifying which default arguments to add
   * @details Controls which standard arguments (help, version) are automatically added
   */
  enum class default_arguments : u8 {
    none    = 0,             ///< No default arguments
    help    = 1,             ///< Add help argument (-h/--help)
    version = 2,             ///< Add version argument (-v/--version)
    all     = help | version ///< Add both help and version arguments
  };

  /**
   * @brief Bitwise AND operator for default_arguments
   * @param a First default_arguments value
   * @param b Second default_arguments value
   * @return Result of bitwise AND operation
   */
  inline fn operator&(const default_arguments& a, const default_arguments& b)->default_arguments {
    return static_cast<default_arguments>(
      std::to_underlying(a) &
      std::to_underlying(b)
    );
  }

  class ArgumentParser;

  /**
   * @brief Class representing a command-line argument
   * @details Handles parsing, validation, and storage of individual command-line arguments
   */
  class Argument {
    friend class ArgumentParser;
    friend fn operator<<(std::ostream& stream, const ArgumentParser& parser)
      ->std::ostream&;

    /**
     * @brief Constructor for Argument with multiple names
     * @tparam N Number of argument names
     * @param prefix_chars Characters that can be used as argument prefixes
     * @param a Array of argument names
     * @param unused Index sequence for parameter pack expansion
     */
    template <usize N, usize... I>
    explicit Argument(const StringView prefix_chars, std::array<StringView, N>&& a, std::index_sequence<I...> /*unused*/) // NOLINT(cppcoreguidelines-rvalue-reference-param-not-moved)
      : m_accepts_optional_like_value(false),
        m_is_optional((is_optional(a[I], prefix_chars) || ...)),
        m_is_required(false),
        m_is_repeatable(false),
        m_is_used(false),
        m_is_hidden(false),
        m_prefix_chars(prefix_chars) {
      ((void)m_names.emplace_back(a[I]), ...);
      std::sort(
        m_names.begin(), m_names.end(), [](const auto& lhs, const auto& rhs) {
          return lhs.size() == rhs.size() ? lhs < rhs : lhs.size() < rhs.size();
        }
      );
    }

   public:
    /**
     * @brief Constructor for Argument with multiple names
     * @tparam N Number of argument names
     * @param prefix_chars Characters that can be used as argument prefixes
     * @param a Array of argument names
     */
    template <usize N>
    explicit Argument(StringView prefix_chars, std::array<StringView, N>&& a)
      : Argument(prefix_chars, std::move(a), std::make_index_sequence<N> {}) {}

    /**
     * @brief Set the help text for this argument
     * @param help_text The help text to display
     * @return Reference to this argument for method chaining
     */
    fn help(String help_text) -> Argument& {
      m_help = std::move(help_text);
      return *this;
    }

    /**
     * @brief Set the metavar (variable name) for this argument
     * @param metavar The metavar to display in help text
     * @return Reference to this argument for method chaining
     */
    fn metavar(String metavar) -> Argument& {
      m_metavar = std::move(metavar);
      return *this;
    }

    /**
     * @brief Set the default value for this argument
     * @tparam T Type of the default value
     * @param value The default value
     * @return Reference to this argument for method chaining
     */
    template <typename T>
    fn default_value(T&& value) -> Argument& {
      m_num_args_range     = NArgsRange { 0, m_num_args_range.get_max() };
      m_default_value_repr = details::repr(value);

      if constexpr (std::is_convertible_v<T, StringView>)
        m_default_value_str = String { StringView { value } };
      else if constexpr (details::can_invoke_to_string<T>::value)
        m_default_value_str = std::to_string(value);

      m_default_value = std::forward<T>(value);
      return *this;
    }

    /**
     * @brief Set the default value for this argument (const char* overload)
     * @param value The default value as a C-style string
     * @return Reference to this argument for method chaining
     */
    fn default_value(const char* value) -> Argument& {
      return default_value(String(value));
    }

    /**
     * @brief Mark this argument as required
     * @return Reference to this argument for method chaining
     */
    fn required() -> Argument& {
      m_is_required = true;
      return *this;
    }

    /**
     * @brief Set the implicit value for this argument
     * @param value The implicit value to use when the argument is present but no value is provided
     * @return Reference to this argument for method chaining
     */
    fn implicit_value(ArgValue value) -> Argument& {
      m_implicit_value = std::move(value);
      m_num_args_range = NArgsRange { 0, 0 };
      return *this;
    }

    /**
     * @brief Configure this argument as a flag (boolean argument)
     * @details Sets default value to false and implicit value to true
     * @return Reference to this argument for method chaining
     */
    fn flag() -> Argument& {
      default_value(false);
      implicit_value(true);
      return *this;
    }

    /**
     * @brief Set a custom action to be performed when this argument is parsed
     * @tparam F Type of the callable object
     * @tparam Args Types of the bound arguments
     * @param callable The function or callable object to invoke
     * @param bound_args Additional arguments to bind to the callable
     * @return Reference to this argument for method chaining
     * @details The callable should accept the argument value as its last parameter
     *          and any bound arguments before it. It can return either void or a Result type.
     */
    template <class F, class... Args>
    fn action(F&& callable, Args&&... bound_args)
      -> Argument&
      requires(std::is_invocable_v<F, Args..., const String>)
    {
      using RawReturnType = std::invoke_result_t<F, Args..., const String>;

      if constexpr (std::is_void_v<RawReturnType>) {
        m_actions.emplace_back<void_action>(
          [f = std::forward<F>(callable), tup = std::make_tuple(std::forward<Args>(bound_args)...)](const String& opt) mutable -> Result<> {
            details::apply_plus_one(f, tup, opt);
            return {};
          }
        );
      } else if constexpr (argparse::details::is_specialization_v<RawReturnType, Result> && std::is_void_v<typename RawReturnType::value_type>) {
        m_actions.emplace_back<void_action>(
          [f = std::forward<F>(callable), tup = std::make_tuple(std::forward<Args>(bound_args)...)](const String& opt) mutable -> Result<> {
            return details::apply_plus_one(f, tup, opt);
          }
        );
      } else if constexpr (argparse::details::is_specialization_v<RawReturnType, Result>) {
        m_actions.emplace_back<valued_action>(
          [f = std::forward<F>(callable), tup = std::make_tuple(std::forward<Args>(bound_args)...)](const String& opt) mutable -> Result<ArgValue> {
            RawReturnType result = details::apply_plus_one(f, tup, opt);
            if (result) {
              if constexpr (!std::is_void_v<typename RawReturnType::value_type>) {
                return result.value();
              } else {
                return ArgValue {};
              }
            } else {
              return Err(result.error());
            }
          }
        );
      } else {
        m_actions.emplace_back<valued_action>(
          [f = std::forward<F>(callable), tup = std::make_tuple(std::forward<Args>(bound_args)...)](const String& opt) mutable -> Result<ArgValue> {
            return details::apply_plus_one(f, tup, opt);
          }
        );
      }
      return *this;
    }

    /**
     * @brief Store the argument value into a boolean variable
     * @param var Reference to the boolean variable to store the value in
     * @return Reference to this argument for method chaining
     * @details If no default or implicit value is set, configures the argument as a flag
     */
    fn store_into(bool& var)
      -> Argument& {
      if ((!m_default_value.has_value()) && (!m_implicit_value.has_value()))
        flag();

      if (m_default_value.has_value())
        var = std::get<bool>(m_default_value.value());

      action([&var](const String& /*unused*/) -> Result<bool> {
        var = true;
        return var;
      });

      return *this;
    }

    /**
     * @brief Store the argument value into an integer variable
     * @tparam T Integer type to store the value in
     * @param var Reference to the variable to store the value in
     * @return Reference to this argument for method chaining
     */
    template <typename T>
    fn store_into(T& var) -> Argument&
      requires(std::is_integral_v<T>)
    {
      if (m_default_value.has_value())
        var = std::get<T>(m_default_value.value());

      action([&var](const auto& s) -> Result<T> {
        Result<T> result = details::parse_number<T, details::radix_10>()(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as decimal integer: {}", s, result.error().message)));

        var = *result;
        return result;
      });

      return *this;
    }

    /**
     * @brief Store the argument value into a floating-point variable
     * @tparam T Floating-point type to store the value in
     * @param var Reference to the variable to store the value in
     * @return Reference to this argument for method chaining
     */
    template <typename T>
    fn store_into(T& var) -> Argument&
      requires(std::is_floating_point_v<T>)
    {
      if (m_default_value.has_value())
        var = std::get<T>(m_default_value.value());

      action([&var](const auto& s) -> Result<T> {
        Result<T> result = details::parse_number<T, details::chars_format::general>()(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as number: {}", s, result.error().message)));

        var = *result;
        return result;
      });

      return *this;
    }

    /**
     * @brief Store the argument value into a string variable
     * @param var Reference to the string variable to store the value in
     * @return Reference to this argument for method chaining
     */
    fn store_into(String& var)
      -> Argument& {
      if (m_default_value.has_value())
        var = std::get<String>(m_default_value.value());

      action([&var](const String& s) -> Result<String> {
        var = s;
        return s;
      });

      return *this;
    }

    /**
     * @brief Store the argument value into a filesystem path variable
     * @param var Reference to the path variable to store the value in
     * @return Reference to this argument for method chaining
     */
    fn store_into(std::filesystem::path& var) -> Argument& {
      if (m_default_value.has_value())
        var = std::get<std::filesystem::path>(m_default_value.value());

      action([&var](const String& s) -> Result<std::filesystem::path> {
        var = s;
        return var;
      });

      return *this;
    }

    /**
     * @brief Store the argument values into a vector of strings
     * @param var Reference to the vector to store the values in
     * @return Reference to this argument for method chaining
     */
    fn store_into(Vec<String>& var) -> Argument& {
      if (m_default_value.has_value())
        var = std::get<Vec<String>>(m_default_value.value());

      action([this, &var](const String& s) -> Result<Vec<String>> {
        if (!m_is_used)
          var.clear();

        m_is_used = true;
        var.push_back(s);
        return var;
      });

      return *this;
    }

    /**
     * @brief Store the argument values into a vector of integers
     * @param var Reference to the vector to store the values in
     * @return Reference to this argument for method chaining
     */
    fn store_into(Vec<int>& var) -> Argument& {
      if (m_default_value.has_value())
        var = std::get<Vec<int>>(m_default_value.value());

      action([this, &var](const String& s) -> Result<Vec<int>> {
        if (!m_is_used)
          var.clear();

        m_is_used = true;

        Result<int> result = details::parse_number<int, details::radix_10>()(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as decimal integer for vector: {}", s, result.error().message)));

        var.push_back(*result);
        return var;
      });

      return *this;
    }

    /**
     * @brief Store the argument values into a set of strings
     * @param var Reference to the set to store the values in
     * @return Reference to this argument for method chaining
     */
    fn store_into(std::set<String>& var) -> Argument& {
      if (m_default_value.has_value())
        var = std::get<std::set<String>>(m_default_value.value());

      action([this, &var](const String& s) -> Result<std::set<String>> {
        if (!m_is_used)
          var.clear();

        m_is_used = true;
        var.insert(s);
        return var;
      });

      return *this;
    }

    /**
     * @brief Store the argument values into a set of integers
     * @param var Reference to the set to store the values in
     * @return Reference to this argument for method chaining
     */
    fn store_into(std::set<int>& var) -> Argument& {
      if (m_default_value.has_value())
        var = std::get<std::set<int>>(m_default_value.value());

      action([this, &var](const String& s) -> Result<std::set<int>> {
        if (!m_is_used)
          var.clear();

        m_is_used = true;

        Result<int> result = details::parse_number<int, details::radix_10>()(s);

        if (!result)
          return Err(DracError(result.error().code, std::format("Failed to parse '{}' as decimal integer for set: {}", s, result.error().message)));

        var.insert(*result);
        return var;
      });

      return *this;
    }

    /**
     * @brief Mark this argument as repeatable
     * @return Reference to this argument for method chaining
     * @details A repeatable argument can be specified multiple times on the command line
     */
    fn append() -> Argument& {
      m_is_repeatable = true;
      return *this;
    }

    /**
     * @brief Mark this argument as hidden
     * @return Reference to this argument for method chaining
     * @details Hidden arguments are not shown in help messages
     */
    fn hidden() -> Argument& {
      m_is_hidden = true;
      return *this;
    }

    /**
     * @brief Configure number parsing format for this argument
     * @tparam Shape Character indicating the number format ('d', 'i', 'u', 'b', 'o', 'x', 'X', 'a', 'A', 'e', 'E', 'f', 'F', 'g', 'G')
     * @tparam T Arithmetic type to parse into
     * @return Reference to this argument for method chaining
     * @details The Shape parameter determines the number format:
     *          - 'd': Decimal integer
     *          - 'i': Integer (auto-detects format)
     *          - 'u': Unsigned decimal integer
     *          - 'b': Binary integer
     *          - 'o': Octal integer
     *          - 'x'/'X': Hexadecimal integer
     *          - 'a'/'A': Hexadecimal floating point
     *          - 'e'/'E': Scientific notation
     *          - 'f'/'F': Fixed point
     *          - 'g'/'G': General format
     */
    template <char Shape, typename T>
    fn scan() -> Argument&
      requires(std::is_arithmetic_v<T>)
    {
      static_assert(!(std::is_const_v<T> || std::is_volatile_v<T>), "T should not be cv-qualified");

      fn is_one_of = [](char c, auto... x) constexpr {
        return ((c == x) || ...);
      };

      if constexpr (Shape == 'd' && std::is_integral_v<T>)
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::radix_10>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as decimal integer (scan 'd'): {}", s, result.error().message)));
          return result;
        });
      else if constexpr (Shape == 'i' && std::is_integral_v<T>)
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as integer (scan 'i'): {}", s, result.error().message)));
          return result;
        });
      else if constexpr (Shape == 'u' && (std::is_integral_v<T> && std::is_unsigned_v<T>))
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::radix_10>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as unsigned decimal integer (scan 'u'): {}", s, result.error().message)));
          return result;
        });
      else if constexpr (Shape == 'b' && (std::is_integral_v<T> && std::is_unsigned_v<T>))
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::radix_2>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as binary integer (scan 'b'): {}", s, result.error().message)));
          return result;
        });
      else if constexpr (Shape == 'o' && (std::is_integral_v<T> && std::is_unsigned_v<T>))
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::radix_8>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as octal integer (scan 'o'): {}", s, result.error().message)));
          return result;
        });
      else if constexpr (is_one_of(Shape, 'x', 'X') && (std::is_integral_v<T> && std::is_unsigned_v<T>))
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::radix_16>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as hexadecimal integer (scan '{}'): {}", s, Shape, result.error().message)));
          return result;
        });
      else if constexpr (is_one_of(Shape, 'a', 'A') && std::is_floating_point_v<T>)
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::chars_format::hex>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as hexadecimal float (scan '{}'): {}", s, Shape, result.error().message)));
          return result;
        });
      else if constexpr (is_one_of(Shape, 'e', 'E') && std::is_floating_point_v<T>)
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::chars_format::scientific>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as scientific float (scan '{}'): {}", s, Shape, result.error().message)));
          return result;
        });
      else if constexpr (is_one_of(Shape, 'f', 'F') && std::is_floating_point_v<T>)
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::chars_format::fixed>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as fixed float (scan '{}'): {}", s, Shape, result.error().message)));
          return result;
        });
      else if constexpr (is_one_of(Shape, 'g', 'G') && std::is_floating_point_v<T>)
        action([](const String& s) -> Result<T> {
          Result<T> result = details::parse_number<T, details::chars_format::general>()(s);
          if (!result)
            return Err(DracError(result.error().code, std::format("Failed to parse '{}' as general float (scan '{}'): {}", s, Shape, result.error().message)));
          return result;
        });
      else
        static_assert(false, "No scan specification for T");

      return *this;
    }

    /**
     * @brief Set the exact number of arguments this argument accepts
     * @param num_args The exact number of arguments required
     * @return Reference to this argument for method chaining
     */
    fn nargs(const usize num_args)
      -> Argument& {
      m_num_args_range = NArgsRange { num_args, num_args };
      return *this;
    }

    /**
     * @brief Set the range of arguments this argument accepts
     * @param num_args_min Minimum number of arguments required
     * @param num_args_max Maximum number of arguments allowed
     * @return Reference to this argument for method chaining
     */
    fn nargs(const usize num_args_min, const usize num_args_max) -> Argument& {
      m_num_args_range = NArgsRange { num_args_min, num_args_max };
      return *this;
    }

    /**
     * @brief Set the number of arguments pattern for this argument
     * @param pattern The pattern to use (optional, any, or at_least_one)
     * @return Reference to this argument for method chaining
     * @details The pattern determines how many arguments are required:
     *          - optional: 0 or 1 arguments
     *          - any: 0 or more arguments
     *          - at_least_one: 1 or more arguments
     */
    fn nargs(const nargs_pattern pattern) -> Argument& {
      switch (pattern) {
        case nargs_pattern::optional:
          m_num_args_range = NArgsRange { 0, 1 };
          break;
        case nargs_pattern::any:
          m_num_args_range = NArgsRange { 0, (std::numeric_limits<std::size_t>::max)() };
          break;
        case nargs_pattern::at_least_one:
          m_num_args_range = NArgsRange { 1, (std::numeric_limits<std::size_t>::max)() };
          break;
      }

      return *this;
    }

    /**
     * @brief Configure this argument to accept all remaining arguments
     * @return Reference to this argument for method chaining
     * @details This is equivalent to nargs(nargs_pattern::any) but also allows
     *          optional arguments to be treated as values
     */
    fn remaining() -> Argument& {
      m_accepts_optional_like_value = true;
      return nargs(nargs_pattern::any);
    }

    /**
     * @brief Add a choice to the list of allowed values
     * @tparam T Type of the choice value
     * @param choice The value to add as a choice
     */
    template <typename T>
    fn add_choice(T&& choice) -> void {
      static_assert(details::IsChoiceTypeSupported<T>::value, "Only string or integer type supported for choice");
      static_assert(std::is_convertible_v<T, StringView> || details::can_invoke_to_string<T>::value, "Choice is not convertible to string_type");
      if (!m_choices.has_value())
        m_choices = std::unordered_set<String> {};

      if constexpr (std::is_convertible_v<T, StringView>)
        m_choices.value().insert(String(StringView { std::forward<T>(choice) }));
      else if constexpr (details::can_invoke_to_string<T>::value)
        m_choices.value().insert(std::to_string(std::forward<T>(choice)));
    }

    /**
     * @brief Get a pointer to this argument if it has choices
     * @return Pointer to this argument or nullptr if no choices have been added
     */
    fn choices() -> Argument* {
      if (!m_choices.has_value() || m_choices.value().empty())
        return nullptr;

      return this;
    }

    /**
     * @brief Add multiple choices to the list of allowed values
     * @tparam T Type of the first choice
     * @tparam U Types of the remaining choices
     * @param first The first choice value
     * @param rest The remaining choice values
     * @return Pointer to this argument or nullptr if no choices have been added
     */
    template <typename T, typename... U>
    fn choices(T&& first, U&&... rest) -> Argument* {
      add_choice(std::forward<T>(first));
      if constexpr (sizeof...(rest) == 0) {
        return choices();
      } else {
        return choices(std::forward<U>(rest)...);
      }
    }

    /**
     * @brief Validate that the default value is in the list of choices
     * @return Result indicating success or failure
     * @details Returns an error if the default value is not in the choices list
     */
    [[nodiscard]] fn find_default_value_in_choices() const -> Result<> {
      assert(m_choices.has_value());
      const auto& choices = m_choices.value();

      if (m_default_value.has_value()) {
        if (!choices.contains(m_default_value_str.value_or(""))) {
          const String choices_as_csv =
            std::accumulate(choices.begin(), choices.end(), String(), [](const String& a, const String& b) {
              return a + (a.empty() ? "" : ", ") + b;
            });

          return Err(DracError(DracErrorCode::InvalidArgument, String { "Invalid default value " } + m_default_value_repr + " - allowed options: {" + choices_as_csv + "}"));
        }
      }

      return {};
    }

    /**
     * @brief Check if a value is in the list of choices
     * @tparam Iterator Type of the iterator pointing to the value
     * @param option_it Iterator pointing to the value to check
     * @return true if the value is in the choices list, false otherwise
     */
    template <typename Iterator>
    [[nodiscard]] fn is_value_in_choices(Iterator option_it) const -> bool {
      assert(m_choices.has_value());
      const auto& choices = m_choices.value();

      return (choices.find(*option_it) != choices.end());
    }

    /**
     * @brief Consume arguments from an iterator range
     * @tparam Iterator Type of the iterator
     * @param start Iterator to the first argument to consume
     * @param end Iterator past the last argument to consume
     * @param used_name The name of the argument being consumed (for error messages)
     * @param dry_run If true, don't actually consume arguments, just check if they can be consumed
     * @return Result containing an iterator to the first unprocessed argument or an error
     * @details This method processes arguments according to the argument's configuration:
     *          - Validates the number of arguments against nargs requirements
     *          - Checks values against choices if specified
     *          - Applies actions to convert and store values
     *          - Handles implicit values for flags
     *          - Manages repeatable arguments
     */
    template <typename Iterator>
    fn consume(Iterator start, Iterator end, const StringView used_name = {}, const bool dry_run = false) -> Result<Iterator> {
      if (!m_is_repeatable && m_is_used)
        return Err(DracError(DracErrorCode::InvalidArgument, String("Duplicate argument ").append(used_name)));

      m_used_name = used_name;

      usize passed_options = 0;

      if (m_choices.has_value()) {
        const auto max_number_of_args = m_num_args_range.get_max();
        const auto min_number_of_args = m_num_args_range.get_min();

        for (auto it = start; it != end; ++it) {
          if (is_value_in_choices(it)) {
            passed_options += 1;
            continue;
          }

          if ((passed_options >= min_number_of_args) &&
              (passed_options <= max_number_of_args))
            break;

          const String choices_as_csv = std::accumulate(
            m_choices.value().begin(), m_choices.value().end(), String(), [](const String& option_a, const String& option_b) {
              return std::format("{}{}{}", option_a, option_a.empty() ? "" : ", ", option_b);
            }
          );

          return Err(DracError(DracErrorCode::InvalidArgument, String { "Invalid argument " } + details::repr(*it) + " - allowed options: {" + choices_as_csv + "}"));
        }
      }

      const usize num_args_max = (m_choices.has_value()) ? passed_options : m_num_args_range.get_max();
      const usize num_args_min = m_num_args_range.get_min();

      if (num_args_max == 0) {
        if (!dry_run) {
          if (m_implicit_value.has_value())
            m_values.emplace_back(*m_implicit_value);

          for (usize i = 0; i < m_actions.size(); ++i) {
            auto&    action = m_actions[i];
            Result<> action_call_result;
            std::visit([&](auto& f) {
              if constexpr (std::is_same_v<decltype(f({})), Result<ArgValue>>) {
                Result<ArgValue> valued_result = f({});
                if (!valued_result)
                  action_call_result = Err(valued_result.error());
              } else {
                action_call_result = f({});
              }
            },
                       action);
            if (!action_call_result)
              return Err(action_call_result.error());
          }

          if (m_actions.empty()) {
            Result<> action_call_result;
            std::visit([&](auto& f) {
              if constexpr (std::is_same_v<decltype(f({})), Result<ArgValue>>) {
                Result<ArgValue> valued_result = f({});
                if (!valued_result)
                  action_call_result = Err(valued_result.error());
              } else {
                action_call_result = f({});
              }
            },
                       m_default_action);
            if (!action_call_result)
              return Err(action_call_result.error());
          }
          m_is_used = true;
        }
        return start;
      }

      if (auto dist = static_cast<usize>(std::distance(start, end)); dist >= num_args_min) {
        if (num_args_max < dist)
          end = std::next(start, static_cast<typename Iterator::difference_type>(num_args_max));

        if (!m_accepts_optional_like_value) {
          end = std::find_if(
            start, end, [this]<typename T>(T&& PH1) { return is_optional(std::forward<T>(PH1), m_prefix_chars); }
          );

          dist = static_cast<usize>(std::distance(start, end));

          if (dist < num_args_min)
            return Err(DracError(DracErrorCode::InvalidArgument, "Too few arguments for '" + String(m_used_name) + "'."));
        }

        struct ActionApply {
          ActionApply(Iterator f, Iterator l, Argument& s)
            : first(f), last(l), self(&s) {}
          Iterator  first, last;
          Argument* self;

          fn operator()(valued_action& f)->Result<> {
            for (auto it_arg = first; it_arg != last; ++it_arg) {
              Result<ArgValue> res = f(*it_arg);
              if (!res)
                return Err(res.error());
              self->m_values.push_back(res.value());
            }
            return {};
          }

          fn operator()(void_action& f)->Result<> {
            for (auto it_arg = first; it_arg != last; ++it_arg) {
              Result<> res = f(*it_arg);
              if (!res)
                return Err(res.error());
            }
            if (!self->m_default_value.has_value())
              if (!self->m_accepts_optional_like_value)
                self->m_values.resize(
                  static_cast<usize>(std::distance(first, last))
                );
            return {};
          }
        };

        if (!dry_run) {
          for (usize i = 0; i < m_actions.size(); ++i) {
            auto&    action       = m_actions[i];
            Result<> apply_result = std::visit(ActionApply { start, end, *this }, action);
            if (!apply_result)
              return Err(apply_result.error());
          }

          if (m_actions.empty()) {
            Result<> apply_result = std::visit(ActionApply { start, end, *this }, m_default_action);
            if (!apply_result)
              return Err(apply_result.error());
          }
          m_is_used = true;
        }

        return end;
      }
      if (m_default_value.has_value()) {
        if (!dry_run)
          m_is_used = true;

        return start;
      }
      return Err(DracError(DracErrorCode::InvalidArgument, std::format("Too few arguments for '{}'", m_used_name)));
    }

    /**
     * @brief Validate the argument's configuration and values
     * @return Result indicating success or failure
     * @details Performs various validation checks:
     *          - Validates nargs range configuration
     *          - Checks required arguments are provided
     *          - Validates number of arguments against requirements
     *          - Verifies values are in choices list if specified
     *          - Validates default values against choices
     */
    [[nodiscard]] fn validate() const -> Result<> {
      if (m_num_args_range.get_min() > m_num_args_range.get_max())
        return Err(DracError(DracErrorCode::InvalidArgument, std::format("Invalid nargs range for argument '{}': min ({}) > max ({}). This indicates a configuration error when defining the argument.", m_names.empty() ? "UnnamedArgument" : m_names[0], m_num_args_range.get_min(), m_num_args_range.get_max())));

      if (m_is_optional) {
        if (!m_is_used && !m_default_value.has_value() && m_is_required)
          return Err(DracError(DracErrorCode::InvalidArgument, std::format("Required argument '{}' was not provided", m_names[0])));

        if (m_is_used && m_is_required && m_values.empty())
          return Err(DracError(DracErrorCode::InvalidArgument, std::format("Required argument '{}' requires a value, but none was provided", m_names[0])));
      } else {
        if (!m_num_args_range.contains(m_values.size()) && !m_default_value.has_value()) {
          String expected_str;
          if (m_num_args_range.is_exact())
            expected_str = std::to_string(m_num_args_range.get_min());
          else if (!m_num_args_range.is_right_bounded())
            expected_str = std::format("at least {}", m_num_args_range.get_min());
          else
            expected_str = std::format("{} to {}", m_num_args_range.get_min(), m_num_args_range.get_max());
          return Err(DracError(DracErrorCode::InvalidArgument, std::format("Incorrect number of arguments for positional argument '{}'. Expected {}, got {}.", (m_metavar.empty() ? m_names[0] : m_metavar), expected_str, m_values.size())));
        }

        if (m_num_args_range.get_max() < m_values.size())
          return Err(DracError(DracErrorCode::InvalidArgument, std::format("Too many arguments for positional argument '{}'. Expected at most {}, got {}.", (m_metavar.empty() ? m_names[0] : m_metavar), m_num_args_range.get_max(), m_values.size())));
      }

      if (m_choices.has_value()) {
        const auto& choices = m_choices.value();

        if (m_default_value.has_value())
          if (const String& default_val_str = m_default_value_str.value(); !choices.contains(default_val_str)) {
            const String choices_as_csv = std::accumulate(
              choices.begin(), choices.end(), String(), [](const String& option_a, const String& option_b) -> String { return option_a + (option_a.empty() ? "" : ", ") + option_b; }
            );
            return Err(DracError(DracErrorCode::InvalidArgument, std::format("Default value '{}' is not in the allowed choices: {{{}}}", default_val_str, choices_as_csv)));
          }
      }

      return {};
    }

    /**
     * @brief Get a comma-separated list of argument names
     * @param separator The separator to use between names
     * @return String containing the names separated by the specified character
     */
    [[nodiscard]] fn get_names_csv(const char separator = ',') const -> String {
      return std::accumulate(
        m_names.begin(), m_names.end(), String { "" }, [&](const String& result, const String& name) {
          return result.empty() ? name : result + separator + name;
        }
      );
    }

    /**
     * @brief Get the full usage string for this argument
     * @return String containing the full usage format
     * @details Includes argument names, metavar, and nargs information
     */
    [[nodiscard]] fn get_usage_full() const -> String {
      std::stringstream usage;

      usage << get_names_csv('/');
      const String metavar = !m_metavar.empty() ? m_metavar : "VAR";

      if (m_num_args_range.get_max() > 0) {
        usage << " " << metavar;
        if (m_num_args_range.get_max() > 1)
          usage << "...";
      }

      return usage.str();
    }

    /**
     * @brief Get the inline usage string for this argument
     * @return String containing the inline usage format
     * @details Includes argument names, metavar, and nargs information in a format
     *          suitable for inline display in help messages
     */
    [[nodiscard]] fn get_inline_usage() const -> String {
      std::stringstream usage;

      String longest_name = m_names.front();
      for (const String& s : m_names)
        if (s.size() > longest_name.size())
          longest_name = s;

      if (!m_is_required)
        usage << "[";

      usage << longest_name;
      const String metavar = !m_metavar.empty() ? m_metavar : "VAR";

      if (m_num_args_range.get_max() > 0) {
        usage << " " << metavar;
        if (m_num_args_range.get_max() > 1 && m_metavar.contains("> <"))
          usage << "...";
      }

      if (!m_is_required)
        usage << "]";

      if (m_is_repeatable)
        usage << "...";

      return usage.str();
    }

    /**
     * @brief Get the length of the argument's display string
     * @return Length of the argument's display string
     * @details Calculates the length needed to display the argument in help messages
     */
    [[nodiscard]] fn get_arguments_length() const -> usize {
      const usize names_size = std::accumulate(
        std::begin(m_names), std::end(m_names), static_cast<usize>(0), [](const u32& sum, const String& s) { return sum + s.size(); }
      );

      if (is_positional(m_names.front(), m_prefix_chars)) {
        if (!m_metavar.empty())
          return 2 + m_metavar.size();

        return 2 + names_size + (m_names.size() - 1);
      }

      usize size = names_size + (2 * (m_names.size() - 1));
      if (!m_metavar.empty() && m_num_args_range == NArgsRange { 1, 1 })
        size += m_metavar.size() + 1;

      return size + 2;
    }

    /**
     * @brief Stream insertion operator for Argument
     * @param stream The output stream to write to
     * @param argument The argument to format and output
     * @return Reference to the output stream
     * @details Formats the argument for display in help messages, including:
     *          - Argument names and metavar
     *          - Help text with proper indentation
     *          - Argument count information
     *          - Default value or required status
     *          - Repeatable status
     */
    friend fn operator<<(std::ostream& stream, const Argument& argument)->std::ostream& {
      String name_str = "  ";

      if (argparse::Argument::is_positional(argument.m_names.front(), argument.m_prefix_chars)) {
        if (!argument.m_metavar.empty()) {
          name_str += argument.m_metavar;
        } else {
          name_str += details::join(argument.m_names.begin(), argument.m_names.end(), " ");
        }
      } else {
        name_str += details::join(argument.m_names.begin(), argument.m_names.end(), ", ");
        if (!argument.m_metavar.empty() &&
            ((argument.m_num_args_range == NArgsRange { 1, 1 }) ||
             (argument.m_num_args_range.get_min() == argument.m_num_args_range.get_max() &&
              argument.m_metavar.contains("> <")))) {
          name_str += std::format(" {}", argument.m_metavar);
        }
      }

      const std::streamsize stream_width = stream.width();
      const String          name_padding = String(name_str.size(), ' ');

      auto pos  = String::size_type {};
      auto prev = String::size_type {};

      bool        first_line = true;
      const char* hspace     = "  ";

      stream << name_str;

      const StringView help_view(argument.m_help);

      while ((pos = argument.m_help.find('\n', prev)) != String::npos) {
        const StringView line = help_view.substr(prev, pos - prev + 1);

        if (first_line) {
          stream << hspace << line;
          first_line = false;
        } else {
          stream.width(stream_width);
          stream << name_padding << hspace << line;
        }

        prev += pos - prev + 1;
      }

      if (first_line)
        stream << hspace << argument.m_help;
      else if (const StringView leftover = help_view.substr(prev, argument.m_help.size() - prev); !leftover.empty()) {
        stream.width(stream_width);
        stream << name_padding << hspace << leftover;
      }

      if (!argument.m_help.empty())
        stream << " ";

      stream << argument.m_num_args_range;

      bool add_space = false;
      if (argument.m_default_value.has_value() &&
          argument.m_num_args_range != NArgsRange { 0, 0 }) {
        stream << std::format("[default: {}]", argument.m_default_value_repr);
        add_space = true;
      } else if (argument.m_is_required) {
        stream << "[required]";
        add_space = true;
      }

      if (argument.m_is_repeatable) {
        if (add_space)
          stream << " ";

        stream << "[may be repeated]";
      }

      stream << "\n";
      return stream;
    }

    /**
     * @brief Inequality comparison operator
     * @tparam T Type of the right-hand side value
     * @param rhs The value to compare against
     * @return true if the argument's value is not equal to rhs
     */
    template <typename T>
    fn operator!=(const T& rhs) const->bool {
      return !(*this == rhs);
    }

    /**
     * @brief Equality comparison operator
     * @tparam T Type of the right-hand side value
     * @param rhs The value to compare against
     * @return true if the argument's value is equal to rhs
     */
    template <typename T>
    fn operator==(const T& rhs) const->bool {
      Result<T> lhs_res = get<T>();
      if (!lhs_res) {
        return false;
      }

      const T& lhs_val = lhs_res.value();

      if constexpr (!details::IsContainer<T>) {
        return lhs_val == rhs;
      } else {
        if (lhs_val.size() != rhs.size()) {
          return false;
        }
        return std::equal(std::begin(lhs_val), std::end(lhs_val), std::begin(rhs));
      }
    }

    /**
     * @brief Check if an argument name represents a positional argument
     * @param name The argument name to check
     * @param prefix_chars Characters that can be used as argument prefixes
     * @return true if the argument is positional, false otherwise
     * @details A positional argument is one that:
     *          - Is empty
     *          - Starts with '-'
     *          - Starts with '-' followed by a decimal literal
     *          - Does not start with a prefix character
     */
    static fn is_positional(StringView name, const StringView prefix_chars) -> bool {
      const int first = lookahead(name);

      if (first == eof)
        return true;

      if (prefix_chars.contains(static_cast<char>(first))) {
        name.remove_prefix(1);

        if (name.empty())
          return true;

        return is_decimal_literal(name);
      }

      return true;
    }

   private:
    /**
     * @brief Class representing a range of allowed argument counts
     * @details Manages the minimum and maximum number of arguments that can be provided
     */
    class NArgsRange {
      usize m_min;
      usize m_max;

     public:
      /**
       * @brief Construct a new NArgsRange object
       * @param minimum Minimum number of arguments allowed
       * @param maximum Maximum number of arguments allowed
       */
      NArgsRange(const usize minimum, const usize maximum)
        : m_min(minimum), m_max(maximum) {}

      /**
       * @brief Check if a value is within the allowed range
       * @param value The value to check
       * @return true if value is between min and max (inclusive)
       */
      [[nodiscard]] fn contains(const usize value) const -> bool {
        return value >= m_min && value <= m_max;
      }

      /**
       * @brief Check if the range represents an exact number of arguments
       * @return true if min equals max
       */
      [[nodiscard]] fn is_exact() const -> bool {
        return m_min == m_max;
      }

      /**
       * @brief Check if the range has an upper bound
       * @return true if max is less than the maximum possible value
       */
      [[nodiscard]] fn is_right_bounded() const -> bool {
        return m_max < (std::numeric_limits<usize>::max)();
      }

      /**
       * @brief Get the minimum number of arguments
       * @return The minimum number of arguments required
       */
      [[nodiscard]] fn get_min() const -> usize {
        return m_min;
      }

      /**
       * @brief Get the maximum number of arguments
       * @return The maximum number of arguments allowed
       */
      [[nodiscard]] fn get_max() const -> usize {
        return m_max;
      }

      /**
       * @brief Stream insertion operator for NArgsRange
       * @param stream The output stream to write to
       * @param range The range to format and output
       * @return Reference to the output stream
       * @details Formats the range as:
       *          - [nargs: N] for exact ranges
       *          - [nargs: N or more] for unbounded ranges
       *          - [nargs=N..M] for bounded ranges
       */
      friend fn operator<<(std::ostream& stream, const NArgsRange& range)
        ->std::ostream& {
        if (range.m_min == range.m_max) {
          if (range.m_min != 0 && range.m_min != 1)
            stream << std::format("[nargs: {}] ", range.m_min);
        } else if (range.m_max == (std::numeric_limits<usize>::max)())
          stream << std::format("[nargs: {} or more] ", range.m_min);
        else
          stream << std::format("[nargs={}..{}] ", range.m_min, range.m_max);

        return stream;
      }

      /**
       * @brief Equality comparison operator
       * @param rhs The range to compare against
       * @return true if both ranges have the same min and max values
       */
      fn operator==(const NArgsRange& rhs) const->bool {
        return rhs.m_min == m_min && rhs.m_max == m_max;
      }

      /**
       * @brief Inequality comparison operator
       * @param rhs The range to compare against
       * @return true if the ranges have different min or max values
       */
      fn operator!=(const NArgsRange& rhs) const->bool {
        return !(*this == rhs);
      }
    };

    static constexpr int eof = std::char_traits<char>::eof();

    /**
     * @brief Get the first character of a string view
     * @param sview The string view to examine
     * @return The first character or EOF if the string is empty
     */
    static fn lookahead(const StringView sview) -> int {
      if (sview.empty())
        return eof;

      return static_cast<unsigned char>(sview[0]);
    }

    /**
     * @brief Check if a string represents a decimal literal
     * @param s The string to check
     * @return true if the string is a valid decimal literal
     * @details A decimal literal can be:
     *          - '0'
     *          - A non-zero digit followed by optional digits
     *          - An integer part followed by a fractional part
     *          - A fractional part
     *          - An integer part followed by '.' and optional exponent
     *          - An integer part followed by an exponent
     */
    // NOLINTBEGIN(cppcoreguidelines-avoid-goto)
    static fn is_decimal_literal(StringView s) -> bool {
      fn is_digit = [](auto c) constexpr -> bool {
        return c >= '0' && c <= '9';
      };

      fn consume_digits = [=](StringView sd) -> StringView {
        const char* const it = std::ranges::find_if_not(sd, is_digit);

        return sd.substr(static_cast<usize>(it - std::begin(sd)));
      };

      switch (lookahead(s)) {
        case '0': {
          s.remove_prefix(1);

          if (s.empty())
            return true;

          goto integer_part;
        }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
          s = consume_digits(s);

          if (s.empty())
            return true;

          goto integer_part_consumed;
        }
        case '.': {
          s.remove_prefix(1);
          goto post_decimal_point;
        }
        default:
          return false;
      }

    integer_part:
      s = consume_digits(s);
    integer_part_consumed:
      switch (lookahead(s)) {
        case '.': {
          s.remove_prefix(1);

          if (is_digit(lookahead(s)))
            goto post_decimal_point;

          goto exponent_part_opt;
        }
        case 'e':
        case 'E': {
          s.remove_prefix(1);
          goto post_e;
        }
        default:
          return false;
      }

    post_decimal_point:
      if (is_digit(lookahead(s))) {
        s = consume_digits(s);
        goto exponent_part_opt;
      }
      return false;

    exponent_part_opt:
      switch (lookahead(s)) {
        case eof:
          return true;
        case 'e':
        case 'E': {
          s.remove_prefix(1);
          goto post_e;
        }
        default:
          return false;
      }

    post_e:
      switch (lookahead(s)) {
        case '-':
        case '+':
          s.remove_prefix(1);
        default:
          break;
      }

      if (is_digit(lookahead(s))) {
        s = consume_digits(s);
        return s.empty();
      }

      return false;
    }
    // NOLINTEND(cppcoreguidelines-avoid-goto)

    /**
     * @brief Check if an argument name represents an optional argument
     * @param name The argument name to check
     * @param prefix_chars Characters that can be used as argument prefixes
     * @return true if the argument is optional, false otherwise
     */
    static fn is_optional(const StringView name, const StringView prefix_chars) -> bool {
      return !is_positional(name, prefix_chars);
    }

    /**
     * @brief Get argument value given a type
     * @tparam T Type of the value to retrieve
     * @return Result containing the value if found, or an error if the type is incompatible
     */
    template <typename T>
    fn get() const -> Result<T> {
      if (!m_values.empty()) {
        try {
          if constexpr (details::IsContainer<T>)
            return argvalue_cast_container<T>(m_values);
          else
            return std::get<T>(m_values.front());
        } catch (const std::bad_any_cast& e) {
          return Err(DracError(DracErrorCode::InternalError, std::format("Bad any_cast for value in get(): {}", e.what())));
        }
      }

      if (m_default_value.has_value()) {
        try {
          return std::get<T>(m_default_value.value());
        } catch (const std::bad_any_cast& e) {
          return Err(DracError(DracErrorCode::InternalError, std::format("Bad any_cast for default_value in get(): {}", e.what())));
        }
      }

      if constexpr (details::IsContainer<T>)
        if (!m_accepts_optional_like_value && m_values.empty())
          return T {};

      return Err(DracError(DracErrorCode::NotFound, std::format("No value provided for '{}'", m_names.back())));
    }

    /**
     * @brief Get argument value given a type
     * @tparam T Type of the value to retrieve
     * @return Result containing the value if found, or an error if the type is incompatible
     */
    template <typename T>
    fn present() const -> Result<Option<T>> {
      if (m_default_value.has_value())
        return Err(DracError(DracErrorCode::InvalidArgument, std::format("present() called on argument '{}' which has a default value.", m_names.back())));

      if (m_values.empty())
        return std::nullopt;

      try {
        if constexpr (details::IsContainer<T>)
          return argvalue_cast_container<T>(m_values);
        else
          return std::get<T>(m_values.front());
      } catch (const std::bad_any_cast& e) {
        return Err(DracError(DracErrorCode::InternalError, std::format("Bad any_cast in present(): {}", e.what())));
      }
    }

    /**
     * @brief Cast a vector of any to a container of a specific type
     * @tparam T Type of the container to cast to
     * @param operand Vector of any to cast
     * @return Container of the specified type
     */
    template <typename T>
    static fn argvalue_cast_container(const Vec<ArgValue>& operand) -> T {
      using ValueType = typename T::value_type;

      T result;

      std::transform(
        std::begin(operand), std::end(operand), std::back_inserter(result), [](const auto& value) { return std::get<ValueType>(value); }
      );

      return result;
    }

    /**
     * @brief Set the usage newline counter
     * @param i New counter value
     */
    fn set_usage_newline_counter(const int i) -> void {
      m_usage_newline_counter = i;
    }

    /**
     * @brief Set the group index
     * @param i New index value
     */
    fn set_group_idx(const usize i) -> void {
      m_group_idx = i;
    }

    /**
     * @brief List of names for this argument (e.g., ["-f", "--file"])
     */
    Vec<String> m_names;

    /**
     * @brief The name that was actually used when parsing this argument
     */
    StringView m_used_name;

    /**
     * @brief Help text describing the purpose and usage of this argument
     */
    String m_help;

    /**
     * @brief Name of the variable to display in help messages (e.g., "FILE" for --file FILE)
     */
    String m_metavar;

    /**
     * @brief Default value for this argument if none is provided
     */
    std::optional<ArgValue> m_default_value;

    /**
     * @brief String representation of the default value for display in help messages
     */
    String m_default_value_repr;

    /**
     * @brief Optional string representation of the default value for validation
     */
    Option<String> m_default_value_str;

    /**
     * @brief Value to use when the argument is present but no value is provided
     */
    std::optional<ArgValue> m_implicit_value;

    /**
     * @brief Optional list of allowed values for this argument
     */
    Option<std::unordered_set<String>> m_choices { std::nullopt };

    /**
     * @brief Type alias for action that returns a value
     */
    using valued_action = std::function<Result<ArgValue>(const String&)>;

    /**
     * @brief Type alias for action that returns void
     */
    using void_action = std::function<Result<>(const String&)>;

    /**
     * @brief List of actions to perform when this argument is parsed
     */
    Vec<std::variant<valued_action, void_action>> m_actions;

    /**
     * @brief Default action to perform if no custom actions are specified
     */
    std::variant<valued_action, void_action> m_default_action {
      std::in_place_type<valued_action>,
      [](const String& value) -> Result<ArgValue> { return value; }
    };

    /**
     * @brief List of values provided for this argument
     */
    Vec<ArgValue> m_values;

    /**
     * @brief Range specifying the allowed number of arguments
     */
    NArgsRange m_num_args_range { 1, 1 };

    /**
     * @brief Whether this argument can accept values that look like optional arguments
     */
    bool m_accepts_optional_like_value : 1;

    /**
     * @brief Whether this argument is optional (starts with a prefix character)
     */
    bool m_is_optional : 1;

    /**
     * @brief Whether this argument must be provided
     */
    bool m_is_required : 1;

    /**
     * @brief Whether this argument can be specified multiple times
     */
    bool m_is_repeatable : 1;

    /**
     * @brief Whether this argument was used in the command line
     */
    bool m_is_used : 1;

    /**
     * @brief Whether this argument should be hidden from help messages
     */
    bool m_is_hidden : 1;

    /**
     * @brief Characters that can be used as argument prefixes (e.g., "-")
     */
    StringView m_prefix_chars;

    /**
     * @brief Counter for tracking newlines in usage messages
     */
    int m_usage_newline_counter = 0;

    /**
     * @brief Index of the group this argument belongs to in help messages
     */
    usize m_group_idx = 0;
  };

  /**
   * @brief Main class for parsing command-line arguments
   *
   * This class provides a comprehensive interface for defining and parsing command-line arguments.
   * It supports both positional and optional arguments, argument groups, subcommands, and more.
   */
  class ArgumentParser {
   public:
    /**
     * @brief Construct a new Argument Parser
     * @param program_name Name of the program (used in help messages)
     * @param version Version string of the program
     * @param add_args Which default arguments to add (help, version, or both)
     * @param exit_on_default_arguments Whether to exit when default arguments are used
     * @param os Output stream for help and version messages
     */
    explicit ArgumentParser(String program_name = {}, String version = "1.0", const default_arguments add_args = default_arguments::all, const bool exit_on_default_arguments = true, std::ostream& os = std::cout)
      : m_program_name(std::move(program_name)), m_version(std::move(version)), m_exit_on_default_arguments(exit_on_default_arguments), m_parser_path(m_program_name) {
      if ((add_args & default_arguments::help) == default_arguments::help)
        add_argument("-h", "--help")
          .action([&](const String& /*unused*/) {
            os << help().str();

            if (m_exit_on_default_arguments)
              std::exit(0);
          })
          .default_value(false)
          .help("shows help message and exits")
          .implicit_value(true)
          .nargs(0);

      if ((add_args & default_arguments::version) == default_arguments::version)
        add_argument("-v", "--version")
          .action([&](const String& /*unused*/) {
            os << m_version << '\n';

            if (m_exit_on_default_arguments)
              std::exit(0);
          })
          .default_value(false)
          .help("prints version information and exits")
          .implicit_value(true)
          .nargs(0);
    }

    ~ArgumentParser() = default;

    ArgumentParser(const ArgumentParser& other)                = delete;
    fn operator=(const ArgumentParser& other)->ArgumentParser& = delete;
    ArgumentParser(ArgumentParser&&) noexcept                  = delete;
    fn operator=(ArgumentParser&&)->ArgumentParser&            = delete;

    /**
     * @brief Check if any arguments were used during parsing
     * @return true if any arguments were used, false otherwise
     */
    explicit operator bool() const {
      const bool arg_used = std::ranges::any_of(m_argument_map, [](auto& it) { return it.second->m_is_used; });

      const bool subparser_used =
        std::ranges::any_of(m_subparser_used, [](auto& it) { return it.second; });

      return m_is_parsed && (arg_used || subparser_used);
    }

    /**
     * @brief Add a new argument to the parser
     * @tparam Targs Types of the argument names
     * @param f_args Argument names (e.g., "-f", "--file")
     * @return Reference to the newly created argument
     */
    template <typename... Targs>
    fn add_argument(Targs... f_args) -> Argument& {
      using array_of_sv = std::array<StringView, sizeof...(Targs)>;

      auto argument = m_optional_arguments.emplace(std::cend(m_optional_arguments), m_prefix_chars, array_of_sv { f_args... });

      if (!argument->m_is_optional)
        m_positional_arguments.splice(std::cend(m_positional_arguments), m_optional_arguments, argument);

      argument->set_usage_newline_counter(m_usage_newline_counter);
      argument->set_group_idx(m_group_names.size());

      index_argument(argument);
      return *argument;
    }

    /**
     * @brief Class representing a group of mutually exclusive arguments
     */
    class MutuallyExclusiveGroup {
      friend class ArgumentParser;

     public:
      MutuallyExclusiveGroup() = delete;

      ~MutuallyExclusiveGroup() = default;

      fn operator=(MutuallyExclusiveGroup&&)->MutuallyExclusiveGroup& = delete;

      /**
       * @brief Construct a new Mutually Exclusive Group
       * @param parent Reference to the parent ArgumentParser
       * @param required Whether at least one argument in the group must be provided
       */
      explicit MutuallyExclusiveGroup(ArgumentParser& parent, const bool required = false)
        : m_parent(parent), m_required(required), m_elements({}) {}

      MutuallyExclusiveGroup(const MutuallyExclusiveGroup& other) = delete;

      fn operator=(const MutuallyExclusiveGroup& other)->MutuallyExclusiveGroup& = delete;

      /**
       * @brief Move constructor for MutuallyExclusiveGroup
       * @param other The other MutuallyExclusiveGroup to move from
       */
      MutuallyExclusiveGroup(MutuallyExclusiveGroup&& other) noexcept
        : m_parent(other.m_parent), m_required(other.m_required), m_elements(std::move(other.m_elements)) {
        other.m_elements.clear();
      }

      /**
       * @brief Add a new argument to the group
       * @tparam Targs Types of the argument names
       * @param f_args Argument names (e.g., "-f", "--file")
       * @return Reference to the newly created argument
       */
      template <typename... Targs>
      fn add_argument(Targs... f_args) -> Argument& {
        Argument& argument = m_parent.add_argument(std::forward<Targs>(f_args)...);
        m_elements.push_back(&argument);
        argument.set_usage_newline_counter(m_parent.m_usage_newline_counter);
        argument.set_group_idx(m_parent.m_group_names.size());
        return argument;
      }

     private:
      /**
       * @brief Reference to the parent ArgumentParser
       */
      ArgumentParser& m_parent; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)

      /**
       * @brief Whether at least one argument in the group must be provided
       */
      bool m_required = false;

      /**
       * @brief Vector of pointers to the arguments in the group
       */
      Vec<Argument*> m_elements;
    };

    /**
     * @brief Add a new mutually exclusive group to the parser
     * @param required Whether at least one argument in the group must be provided
     * @return Reference to the newly created group
     */
    fn add_mutually_exclusive_group(bool required = false) -> MutuallyExclusiveGroup& {
      m_mutually_exclusive_groups.emplace_back(*this, required);
      return m_mutually_exclusive_groups.back();
    }

    /**
     * @brief Add parent parsers to the current parser
     * @tparam Targs Types of the parent parsers
     * @param f_args Parent parsers to add
     * @return Reference to the current parser
     */
    template <typename... Targs>
    fn add_parents(const Targs&... f_args) -> ArgumentParser& {
      for (const ArgumentParser& parent_parser : { std::ref(f_args)... }) {
        for (const Argument& argument : parent_parser.m_positional_arguments) {
          const auto it = m_positional_arguments.insert(
            std::cend(m_positional_arguments), argument
          );

          index_argument(it);
        }

        for (const Argument& argument : parent_parser.m_optional_arguments) {
          const auto it = m_optional_arguments.insert(std::cend(m_optional_arguments), argument);

          index_argument(it);
        }
      }

      return *this;
    }

    /**
     * @brief Ask for the next optional arguments to be displayed on a separate
     * line in usage() output. Only effective if set_usage_max_line_width() is
     * also used.
     */
    fn add_usage_newline() -> ArgumentParser& {
      ++m_usage_newline_counter;
      return *this;
    }

    /**
     * @brief Ask for the next optional arguments to be displayed in a separate section
     * in usage() and help (<< *this) output.
     * For usage(), this is only effective if set_usage_max_line_width() is
     * also used.
     */
    fn add_group(String group_name) -> ArgumentParser& {
      m_group_names.emplace_back(std::move(group_name));
      return *this;
    }

    /**
     * @brief Add a description to the parser
     * @param description Description to add
     * @return Reference to the current parser
     */
    fn add_description(String description) -> ArgumentParser& {
      m_description = std::move(description);
      return *this;
    }

    /**
     * @brief Add an epilog to the parser
     * @param epilog Epilog to add
     * @return Reference to the current parser
     */
    fn add_epilog(String epilog) -> ArgumentParser& {
      m_epilog = std::move(epilog);
      return *this;
    }

    /**
     * @brief Add a un-documented/hidden alias for an argument
     * @param arg Argument to alias
     * @param alias Alias to add
     * @return Reference to the current parser, or an error if the argument is not an optional argument of this parser
     */
    fn add_hidden_alias_for(const Argument& arg, const StringView alias) -> Result<ArgumentParser*> {
      for (auto it = m_optional_arguments.begin();
           it != m_optional_arguments.end();
           ++it)
        if (&(*it) == &arg) {
          m_argument_map.insert_or_assign(String(alias), it);
          return this;
        }

      return Err(DracError(DracErrorCode::InvalidArgument, std::format("Argument is not an optional argument of this parser")));
    }

    /**
     * @brief Getter for arguments and subparsers
     * @tparam T Type of the argument or subparser
     * @param name Name of the argument or subparser
     * @return Reference to the argument or subparser, or an error if the name is invalid
     */
    template <typename T = Argument>
    fn at(const StringView name) -> Result<T*> {
      if constexpr (std::is_same_v<T, Argument>) {
        Result<Argument*> arg_result = (*this)[name];
        if (!arg_result) {
          return Err(DracError(DracErrorCode::NotFound, std::format("Argument not found in 'at': {}", name)));
        }
        return arg_result.value();
      } else {
        static_assert(std::is_same_v<T, ArgumentParser>, "T must be Argument or ArgumentParser for at()");
        const String str_name(name);
        if (const auto subparser_it = m_subparser_map.find(str_name); subparser_it != m_subparser_map.end()) {
          return &(subparser_it->second->get());
        }
        return Err(DracError(DracErrorCode::NotFound, std::format("No such subparser: {}", str_name)));
      }
    }

    /**
     * @brief Set the prefix characters for the parser
     * @param prefix_chars Prefix characters to set
     * @return Reference to the current parser
     */
    fn set_prefix_chars(String prefix_chars) -> ArgumentParser& {
      m_prefix_chars = std::move(prefix_chars);
      return *this;
    }

    /**
     * @brief Set the assign characters for the parser
     * @param assign_chars Assign characters to set
     * @return Reference to the current parser
     */
    fn set_assign_chars(String assign_chars) -> ArgumentParser& {
      m_assign_chars = std::move(assign_chars);
      return *this;
    }

    /**
     * @brief Call parse_args_internal - which does all the work
     * Then, validate the parsed arguments
     * This variant is used mainly for testing
     * @return void, or an error if the arguments are invalid
     */
    // NOLINTNEXTLINE(misc-no-recursion)
    fn parse_args(const Vec<String>& arguments) -> Result<> {
      Result<> pres = parse_args_internal(arguments);
      if (!pres)
        return pres;

      for (const auto& argument_entry : m_argument_map) {
        if (Result<> validation_result = argument_entry.second->validate(); !validation_result) {
          return Err(validation_result.error());
        }
      }

      for (const MutuallyExclusiveGroup& group : m_mutually_exclusive_groups) {
        bool            mutex_argument_used = false;
        const Argument* mutex_argument_ptr  = nullptr;

        for (const Argument* arg : group.m_elements) {
          auto is_used_res = this->is_used(arg->m_names.front());
          if (!is_used_res)
            return Err(is_used_res.error());

          if (!mutex_argument_used && is_used_res.value()) {
            mutex_argument_used = true;
            mutex_argument_ptr  = arg;
          } else if (mutex_argument_used && is_used_res.value()) {
            return Err(DracError(DracErrorCode::InvalidArgument, std::format("Argument '{}' not allowed with '{}'", arg->get_usage_full(), mutex_argument_ptr->get_usage_full())));
          }
        }

        if (!mutex_argument_used && group.m_required) {
          String      argument_names {};
          usize       i    = 0;
          const usize size = group.m_elements.size();
          for (const Argument* arg : group.m_elements) {
            if (i + 1 == size)
              argument_names += std::format("'{}' ", arg->get_usage_full());
            else
              argument_names += std::format("'{}' or ", arg->get_usage_full());
            i += 1;
          }
          return Err(DracError(DracErrorCode::InvalidArgument, std::format("One of the arguments {}is required", argument_names)));
        }
      }
      return {};
    }

    /**
     * @brief Call parse_known_args_internal - which does all the work
     * Then, validate the parsed arguments
     * This variant is used mainly for testing
     * @return void, or an error if the arguments are invalid
     */
    // NOLINTNEXTLINE(misc-no-recursion)
    fn parse_known_args_internal(const Vec<String>& raw_arguments) -> Result<Vec<String>> {
      Vec<String> arguments = preprocess_arguments(raw_arguments);
      Vec<String> unknown_arguments {};

      if (m_program_name.empty() && !arguments.empty())
        m_program_name = arguments.front();

      const auto end                    = std::end(arguments);
      auto       positional_argument_it = std::begin(m_positional_arguments);

      for (auto it = std::next(std::begin(arguments)); it != end;) {
        const String& current_argument = *it;
        if (Argument::is_positional(current_argument, m_prefix_chars)) {
          if (positional_argument_it == std::end(m_positional_arguments)) {
            if (auto subparser_it = m_subparser_map.find(current_argument); subparser_it != m_subparser_map.end()) {
              const Vec<String> unprocessed_arguments = Vec<String>(it, end);
              m_is_parsed                             = true;
              m_subparser_used[current_argument]      = true;
              return subparser_it->second->get().parse_known_args_internal(unprocessed_arguments);
            }
            unknown_arguments.push_back(current_argument);
            ++it;
          } else {
            const auto           argument       = positional_argument_it++;
            Result<decltype(it)> consume_result = argument->consume(it, end);
            if (!consume_result)
              return Err(consume_result.error());
            it = consume_result.value();
          }
          continue;
        }

        auto arg_map_it = m_argument_map.find(current_argument);
        if (arg_map_it != m_argument_map.end()) {
          const auto           argument       = arg_map_it->second;
          Result<decltype(it)> consume_result = argument->consume(std::next(it), end, arg_map_it->first);
          if (!consume_result)
            return Err(consume_result.error());
          it = consume_result.value();
        } else if (const String& compound_arg = current_argument;
                   compound_arg.size() > 1 &&
                   is_valid_prefix_char(compound_arg[0]) &&
                   !is_valid_prefix_char(compound_arg[1])) {
          ++it;
          for (usize j = 1; j < compound_arg.size(); j++) {
            const String hypothetical_arg = { '-', compound_arg[j] };
            auto         arg_map_it2      = m_argument_map.find(hypothetical_arg);
            if (arg_map_it2 != m_argument_map.end()) {
              const auto           argument       = arg_map_it2->second;
              Result<decltype(it)> consume_result = argument->consume(it, end, arg_map_it2->first);
              if (!consume_result)
                return Err(consume_result.error());
              it = consume_result.value();
            } else {
              unknown_arguments.push_back(current_argument);
              break;
            }
          }
        } else {
          unknown_arguments.push_back(current_argument);
          ++it;
        }
      }
      m_is_parsed = true;
      return unknown_arguments;
    }

    /**
     * @brief Main entry point for parsing command-line arguments using this
     * ArgumentParser
     * @return void, or an error if the arguments are invalid
     */
    // NOLINTNEXTLINE(*-avoid-c-arrays)
    fn parse_args(const int argc, const char* const argv[]) -> Result<> {
      return parse_args({ argv, argv + argc });
    }

    /**
     * @brief Main entry point for parsing command-line arguments using this
     * ArgumentParser
     * @return a vector of unknown arguments, or an error if the arguments are invalid
     */
    // NOLINTNEXTLINE(*-avoid-c-arrays)
    fn parse_known_args(const int argc, const char* const argv[]) -> Result<Vec<String>> {
      return parse_known_args_internal({ argv, argv + argc });
    }

    /**
     * @brief Getter for options with default values
     * @return the option value, or an error if the option is not found or has no value
     */
    template <typename T = String>
    fn get(const StringView arg_name) const -> Result<T> {
      if (!m_is_parsed)
        return Err(DracError(DracErrorCode::InternalError, "Nothing parsed, no arguments are available."));

      Result<Argument*> arg_ref_result = (*this)[arg_name];
      if (!arg_ref_result)
        return Err(arg_ref_result.error());

      return arg_ref_result.value()->get<T>();
    }

    /**
     * @brief Getter for options without default values
     * @pre The option has no default value
     * @return the option value, or an error if the option is not found or has no value
     */
    template <typename T = String>
    fn present(const StringView arg_name) const -> Result<Option<T>> {
      if (!m_is_parsed)
        return Err(DracError(DracErrorCode::InternalError, "Nothing parsed, no arguments are available for present()."));

      Result<Argument*> arg_ref_result = (*this)[arg_name];
      if (!arg_ref_result)
        return Err(arg_ref_result.error());

      return arg_ref_result.value()->present<T>();
    }

    /**
     * @brief Getter that returns true for user-supplied options
     * @return true if the option is user-supplied, false otherwise
     */
    [[nodiscard]] fn is_used(const StringView arg_name) const -> Result<bool> {
      if (!m_is_parsed)
        return Err(DracError(DracErrorCode::InternalError, "Nothing parsed, cannot check if argument is used."));

      Result<Argument*> arg_ref_result = (*this)[arg_name];
      if (!arg_ref_result)
        return Err(arg_ref_result.error());
      return bool { arg_ref_result.value()->m_is_used };
    }

    /**
     * @brief Getter that returns true if a subcommand is used
     * @param subcommand_name Subcommand name to check
     * @return true if subcommand is used, false otherwise, or an error if the subcommand is not found
     */
    [[nodiscard]] fn is_subcommand_used(const StringView subcommand_name) const -> Result<bool> {
      if (!m_is_parsed)
        return Err(DracError(DracErrorCode::InternalError, "Nothing parsed, cannot check if subcommand is used."));
      try {
        return m_subparser_used.at(String(subcommand_name));
      } catch (const std::out_of_range& oor) {
        return Err(DracError(DracErrorCode::NotFound, std::format("Subcommand '{}' not found for is_subcommand_used check.", subcommand_name)));
      }
    }

    /**
     * @brief Getter that returns true if a subcommand is used
     * @param subparser Subparser to check
     * @return true if subcommand is used, false otherwise, or an error if the subcommand is not found
     */
    [[nodiscard]] fn is_subcommand_used(const ArgumentParser& subparser) const -> Result<bool> {
      return is_subcommand_used(subparser.m_program_name);
    }

    /**
     * @brief Indexing operator
     * @param arg_name Argument name to check
     * @return a reference to the argument, or an error if the argument is not found
     */
    fn operator[](const StringView arg_name) const->Result<Argument*> {
      String name(arg_name);

      auto it = m_argument_map.find(name);

      if (it != m_argument_map.end())
        return &(*(it->second));

      if (!is_valid_prefix_char(arg_name.front())) {
        const char legal_prefix_char = get_any_valid_prefix_char();

        const String prefix = String(1, legal_prefix_char);

        name = std::format("{}{}", prefix, arg_name);
        it   = m_argument_map.find(name);
        if (it != m_argument_map.end())
          return &(*(it->second));

        name = std::format("{}{}", prefix, name);
        it   = m_argument_map.find(name);

        if (it != m_argument_map.end())
          return &(*(it->second));
      }

      return Err(DracError(DracErrorCode::NotFound, std::format("No such argument: {}", arg_name)));
    }

    /**
     * @brief Print help message
     * @param stream Output stream
     * @param parser ArgumentParser to print
     * @return Output stream
     */
    friend fn operator<<(std::ostream& stream, const ArgumentParser& parser)->std::ostream& {
      stream.setf(std::ios_base::left);

      const usize longest_arg_length = parser.get_length_of_longest_argument();

      stream << parser.usage() << "\n\n";

      if (!parser.m_description.empty())
        stream << parser.m_description << "\n\n";

      const bool has_visible_positional_args = std::ranges::find_if(parser.m_positional_arguments, [](const Argument& argument) { return !argument.m_is_hidden; }) !=
        parser.m_positional_arguments.end();
      if (has_visible_positional_args)
        stream << "Positional arguments:\n";

      for (const Argument& argument : parser.m_positional_arguments)
        if (!argument.m_is_hidden) {
          stream.width(static_cast<std::streamsize>(longest_arg_length));
          stream << argument;
        }

      if (!parser.m_optional_arguments.empty())
        stream << (!has_visible_positional_args ? "" : "\n")
               << "Optional arguments:\n";

      for (const Argument& argument : parser.m_optional_arguments)
        if (argument.m_group_idx == 0 && !argument.m_is_hidden) {
          stream.width(static_cast<std::streamsize>(longest_arg_length));
          stream << argument;
        }

      for (usize i_group = 0; i_group < parser.m_group_names.size(); ++i_group) {
        stream << std::format("\n{} (detailed usage):\n", parser.m_group_names[i_group]);

        for (const Argument& argument : parser.m_optional_arguments)
          if (argument.m_group_idx == i_group + 1 && !argument.m_is_hidden) {
            stream.width(static_cast<std::streamsize>(longest_arg_length));
            stream << argument;
          }
      }

      if (std::ranges::any_of(parser.m_subparser_map, [](auto& p) { return !p.second->get().m_suppress; })) {
        stream << (parser.m_positional_arguments.empty()
                     ? (parser.m_optional_arguments.empty() ? "" : "\n")
                     : "\n")
               << "Subcommands:\n";
        for (const auto& [command, subparser] : parser.m_subparser_map) {
          if (subparser->get().m_suppress)
            continue;

          stream << std::format("  {:<{}} {}", command, longest_arg_length - 2, subparser->get().m_description) << "\n";
        }
      }

      if (!parser.m_epilog.empty()) {
        stream << '\n';
        stream << parser.m_epilog << "\n\n";
      }

      return stream;
    }

    /**
     * @brief Format help message
     * @return Help message
     */
    [[nodiscard]] fn help() const -> std::stringstream {
      std::stringstream out;
      out << *this;
      return out;
    }

    /**
     * @brief Set the maximum width for a line of the Usage message
     * @param w Maximum width
     * @return Reference to the current parser
     */
    fn set_usage_max_line_width(const usize w) -> ArgumentParser& {
      this->m_usage_max_line_width = w;
      return *this;
    }

    /**
     * @brief Asks to display arguments of mutually exclusive group on separate lines in
     * the Usage message
     * @return Reference to the current parser
     */
    fn set_usage_break_on_mutex() -> ArgumentParser& {
      this->m_usage_break_on_mutex = true;
      return *this;
    }

    /**
     * @brief Format usage part of help only
     * @return Usage message
     */
    [[nodiscard]] fn usage() const -> String {
      String     curline = std::format("Usage: {}", this->m_parser_path);
      const bool multiline_usage =
        this->m_usage_max_line_width < (std::numeric_limits<usize>::max)();
      const usize indent_size = curline.size();
      String      result;

      const fn deal_with_options_of_group = [&](const usize group_idx) {
        bool found_options = false;

        const MutuallyExclusiveGroup* cur_mutex             = nullptr;
        int                           usage_newline_counter = -1;

        for (const Argument& argument : this->m_optional_arguments) {
          if (argument.m_is_hidden) {
            continue;
          }
          if (multiline_usage) {
            if (argument.m_group_idx != group_idx) {
              continue;
            }
            if (usage_newline_counter != argument.m_usage_newline_counter) {
              if (usage_newline_counter >= 0) {
                if (curline.size() > indent_size) {
                  result += std::format("\n{}", curline);
                  curline = String(indent_size, ' ');
                }
              }
              usage_newline_counter = argument.m_usage_newline_counter;
            }
          }
          found_options                                  = true;
          const String                  arg_inline_usage = argument.get_inline_usage();
          const MutuallyExclusiveGroup* arg_mutex =
            get_belonging_mutex(&argument);
          if ((cur_mutex != nullptr) && (arg_mutex == nullptr)) {
            curline += ']';
            if (this->m_usage_break_on_mutex) {
              result += std::format("\n{}", curline);
              curline = String(indent_size, ' ');
            }
          } else if ((cur_mutex == nullptr) && (arg_mutex != nullptr)) {
            if ((this->m_usage_break_on_mutex && curline.size() > indent_size) ||
                curline.size() + 3 + arg_inline_usage.size() >
                  this->m_usage_max_line_width) {
              result += std::format("\n{}", curline);
              curline = String(indent_size, ' ');
            }
            curline += " [";
          } else if ((cur_mutex != nullptr) && (arg_mutex != nullptr)) {
            if (cur_mutex != arg_mutex) {
              curline += ']';
              if (this->m_usage_break_on_mutex ||
                  curline.size() + 3 + arg_inline_usage.size() >
                    this->m_usage_max_line_width) {
                result += std::format("\n{}", curline);
                curline = String(indent_size, ' ');
              }
              curline += " [";
            } else {
              curline += '|';
            }
          }
          cur_mutex = arg_mutex;
          if (curline.size() != indent_size &&
              curline.size() + 1 + arg_inline_usage.size() >
                this->m_usage_max_line_width) {
            result += std::format("\n{}", curline);
            curline = String(indent_size, ' ');
            curline += " ";
          } else if (cur_mutex == nullptr) {
            curline += " ";
          }
          curline += arg_inline_usage;
        }
        if (cur_mutex != nullptr) {
          curline += ']';
        }
        return found_options;
      };

      if (const bool found_options = deal_with_options_of_group(0); found_options && multiline_usage &&
          !this->m_positional_arguments.empty()) {
        result += std::format("\n{}", curline);
        curline = String(indent_size, ' ');
      }

      for (const Argument& argument : this->m_positional_arguments) {
        if (argument.m_is_hidden)
          continue;

        const String pos_arg = !argument.m_metavar.empty()
          ? argument.m_metavar
          : argument.m_names.front();

        if (curline.size() + 1 + pos_arg.size() > this->m_usage_max_line_width) {
          result += std::format("\n{}", curline);
          curline = String(indent_size, ' ');
        }

        curline += " ";

        if (argument.m_num_args_range.get_min() == 0 &&
            !argument.m_num_args_range.is_right_bounded()) {
          curline += "[";
          curline += pos_arg;
          curline += "]...";
        } else if (argument.m_num_args_range.get_min() == 1 &&
                   !argument.m_num_args_range.is_right_bounded()) {
          curline += pos_arg;
          curline += "...";
        } else
          curline += pos_arg;
      }

      if (multiline_usage)
        for (usize i = 0; i < m_group_names.size(); ++i) {
          result += std::format("\n\n{}:\n", m_group_names[i]);
          curline = String(indent_size, ' ');
          deal_with_options_of_group(i + 1);
        }

      result += curline;

      if (!m_subparser_map.empty()) {
        result += " {";
        usize i { 0 };
        for (const auto& [command, subparser] : m_subparser_map) {
          if (subparser->get().m_suppress)
            continue;

          if (i == 0)
            result += command;
          else
            result += std::format(",{}", command);

          ++i;
        }
        result += "}";
      }

      return result;
    }

    /**
     * @brief Add a subparser to the parser
     * @param parser Subparser to add
     */
    fn add_subparser(ArgumentParser& parser) -> void {
      parser.m_parser_path = m_program_name + " " + parser.m_program_name;

      auto it = m_subparsers.emplace(std::cend(m_subparsers), parser);

      m_subparser_map.insert_or_assign(parser.m_program_name, it);
      m_subparser_used.insert_or_assign(parser.m_program_name, false);
    }

    /**
     * @brief Set suppress
     * @param suppress Suppress
     */
    fn set_suppress(const bool suppress) -> void {
      m_suppress = suppress;
    }

   protected:
    /**
     * @brief Get the belonging mutex
     * @param arg Argument
     * @return Belonging mutex
     */
    fn get_belonging_mutex(const Argument* arg) const -> const MutuallyExclusiveGroup* {
      for (const MutuallyExclusiveGroup& mutex : m_mutually_exclusive_groups)
        if (std::ranges::find(mutex.m_elements, arg) !=
            mutex.m_elements.end())
          return &mutex;

      return nullptr;
    }

    /**
     * @brief Check if a character is a valid prefix character
     * @param c Character
     * @return True if valid, false otherwise
     */
    [[nodiscard]] fn is_valid_prefix_char(const char c) const -> bool {
      return m_prefix_chars.contains(c);
    }

    /**
     * @brief Get any valid prefix character
     * @return Any valid prefix character
     */
    [[nodiscard]] fn get_any_valid_prefix_char() const -> char {
      return m_prefix_chars[0];
    }

    /**
     * @brief Pre-process this argument list
     * @param raw_arguments Raw arguments
     * @return Pre-processed arguments
     */
    [[nodiscard]] fn preprocess_arguments(const Vec<String>& raw_arguments) const -> Vec<String> {
      Vec<String> arguments {};
      for (const String& arg : raw_arguments) {
        const auto argument_starts_with_prefix_chars =
          [this](const String& a) -> bool {
          if (!a.empty()) {
            // Windows-style
            // if '/' is a legal prefix char
            // then allow single '/' followed by argument name, followed by an
            // assign char, e.g., ':' e.g., 'test.exe /A:Foo'
            if (is_valid_prefix_char('/')) {
              if (is_valid_prefix_char(a[0]))
                return true;
            } else
              // Slash '/' is not a legal prefix char
              // For all other characters, only support long arguments
              // i.e., the argument must start with 2 prefix chars, e.g,
              // '--foo' e,g, './test --foo=Bar -DARG=yes'
              if (a.size() > 1)
                return (is_valid_prefix_char(a[0]) && is_valid_prefix_char(a[1]));
          }

          return false;
        };

        // Check that:
        // - We don't have an argument named exactly this
        // - The argument starts with a prefix char, e.g., "--"
        // - The argument contains an assign char, e.g., "="

        if (const usize assign_char_pos = arg.find_first_of(m_assign_chars); !m_argument_map.contains(arg) &&
            argument_starts_with_prefix_chars(arg) &&
            assign_char_pos != String::npos)
          // Get the name of the potential option, and check it exists
          if (String opt_name = arg.substr(0, assign_char_pos); m_argument_map.contains(opt_name)) {
            // This is the name of an option! Split it into two parts
            arguments.push_back(std::move(opt_name));
            arguments.push_back(arg.substr(assign_char_pos + 1));
            continue;
          }

        // If we've fallen through to here, then it's a standard argument
        arguments.push_back(arg);
      }

      return arguments;
    }

    /**
     * @brief Parse arguments
     * @param raw_arguments Raw arguments
     * @return void, or an error if the arguments are invalid
     */
    // NOLINTNEXTLINE(misc-no-recursion)
    fn parse_args_internal(const Vec<String>& raw_arguments) -> Result<> {
      Vec<String> arguments = preprocess_arguments(raw_arguments);

      if (m_program_name.empty() && !arguments.empty())
        m_program_name = arguments.front();

      auto end                    = std::end(arguments);
      auto positional_argument_it = std::begin(m_positional_arguments);

      for (auto it = std::next(std::begin(arguments)); it != end;) {
        const String& current_argument = *it;
        if (Argument::is_positional(current_argument, m_prefix_chars)) {
          if (positional_argument_it == std::end(m_positional_arguments)) {
            if (const auto subparser_it = m_subparser_map.find(current_argument); subparser_it != m_subparser_map.end()) {
              const Vec<String> unprocessed_arguments = Vec<String>(it, end);
              m_is_parsed                             = true;
              m_subparser_used[current_argument]      = true;
              Result<> sub_parse_res                  = subparser_it->second->get().parse_args_internal(unprocessed_arguments);
              if (!sub_parse_res)
                return sub_parse_res;
              return {};
            }

            if (m_positional_arguments.empty()) {
              if (!m_subparser_map.empty())
                return Err(DracError(DracErrorCode::InvalidArgument, std::format("Failed to parse '{}', did you mean '{}'", current_argument, details::get_most_similar_string(m_subparser_map, current_argument))));
              if (!m_optional_arguments.empty()) {
                for (const Argument& opt : m_optional_arguments) {
                  if (!opt.m_implicit_value.has_value()) {
                    if (!opt.m_is_used) {
                      return Err(DracError(DracErrorCode::InvalidArgument, std::format("Zero positional arguments expected, did you mean '{}'", opt.get_usage_full())));
                    }
                  }
                }
                return Err(DracError(DracErrorCode::InvalidArgument, "Zero positional arguments expected"));
              }
              return Err(DracError(DracErrorCode::InvalidArgument, "Zero positional arguments expected"));
            }
            return Err(DracError(DracErrorCode::InvalidArgument, std::format("Maximum number of positional arguments exceeded, failed to parse '{}'", current_argument)));
          }

          const auto argument_ptr = positional_argument_it++;
          if (argument_ptr->m_num_args_range.get_min() == 1 &&
              argument_ptr->m_num_args_range.get_max() == (std::numeric_limits<usize>::max)() &&
              positional_argument_it != std::end(m_positional_arguments) &&
              std::next(positional_argument_it) == std::end(m_positional_arguments) &&
              positional_argument_it->m_num_args_range.get_min() == 1 &&
              positional_argument_it->m_num_args_range.get_max() == 1) {
            if (std::next(it) != end) {
              Result<decltype(end)> consume_res = positional_argument_it->consume(std::prev(end), end);
              if (!consume_res)
                return Err(consume_res.error());
              end = std::prev(end);
            } else
              return Err(DracError(DracErrorCode::InvalidArgument, std::format("Missing {}", positional_argument_it->m_names.front())));
          }

          Result<decltype(it)> consume_result = argument_ptr->consume(it, end);
          if (!consume_result)
            return Err(consume_result.error());
          it = consume_result.value();
          continue;
        }

        auto arg_map_it = m_argument_map.find(current_argument);
        if (arg_map_it != m_argument_map.end()) {
          const auto           argument_iter  = arg_map_it->second;
          Result<decltype(it)> consume_result = argument_iter->consume(std::next(it), end, arg_map_it->first);
          if (!consume_result)
            return Err(consume_result.error());
          it = consume_result.value();
        } else if (const String& compound_arg = current_argument;
                   compound_arg.size() > 1 &&
                   is_valid_prefix_char(compound_arg[0]) &&
                   !is_valid_prefix_char(compound_arg[1])) {
          ++it;
          for (usize j = 1; j < compound_arg.size(); j++) {
            const String hypothetical_arg = { '-', compound_arg[j] };
            auto         arg_map_it2      = m_argument_map.find(hypothetical_arg);
            if (arg_map_it2 != m_argument_map.end()) {
              auto argument = arg_map_it2->second;
              if (argument->m_num_args_range.get_max() == 0) {
                // Flag: do not consume the next argument as a value
                Result<decltype(it)> consume_result_flag = argument->consume(it, it, arg_map_it2->first);
                if (!consume_result_flag)
                  return Err(consume_result_flag.error());
                it = consume_result_flag.value();
              } else {
                // Option expects a value: consume as before
                Result<decltype(it)> consume_result = argument->consume(it, end, arg_map_it2->first);
                if (!consume_result)
                  return Err(consume_result.error());
              }
            } else {
              return Err(DracError(DracErrorCode::InvalidArgument, std::format("Unknown argument: {}", current_argument)));
            }
          }
        } else
          return Err(DracError(DracErrorCode::InvalidArgument, std::format("Unknown argument: {}", current_argument)));
      }
      m_is_parsed = true;
      return {};
    }

    /**
     * @brief Get the length of the longest argument
     * @return Length of the longest argument
     */
    [[nodiscard]] fn get_length_of_longest_argument() const -> usize {
      if (m_argument_map.empty())
        return 0;

      usize max_size = 0;

      for (const auto& argument : m_argument_map | std::views::values)
        max_size =
          std::max<usize>(max_size, argument->get_arguments_length());

      for (const String& command : m_subparser_map | std::views::keys)
        max_size = std::max<usize>(max_size, command.size());

      return max_size;
    }

    using argument_it    = std::list<Argument>::iterator;
    using mutex_group_it = Vec<MutuallyExclusiveGroup>::iterator;
    using argument_parser_it =
      std::list<std::reference_wrapper<ArgumentParser>>::iterator;

    /**
     * @brief Index argument
     * @param it Argument iterator
     */
    fn index_argument(argument_it it) -> void {
      for (const String& name : std::as_const(it->m_names))
        m_argument_map.insert_or_assign(name, it);
    }

   private:
    String                                            m_program_name;                                                ///< Program name
    String                                            m_version;                                                     ///< Version
    String                                            m_description;                                                 ///< Description
    String                                            m_epilog;                                                      ///< Epilog
    bool                                              m_exit_on_default_arguments = true;                            ///< Exit on default arguments
    String                                            m_prefix_chars { "-" };                                        ///< Prefix characters
    String                                            m_assign_chars { "=" };                                        ///< Assign characters
    bool                                              m_is_parsed = false;                                           ///< Whether the arguments have been parsed
    std::list<Argument>                               m_positional_arguments;                                        ///< Positional arguments
    std::list<Argument>                               m_optional_arguments;                                          ///< Optional arguments
    std::unordered_map<String, argument_it>           m_argument_map;                                                ///< Argument map
    String                                            m_parser_path;                                                 ///< Parser path
    std::list<std::reference_wrapper<ArgumentParser>> m_subparsers;                                                  ///< Subparsers
    std::unordered_map<String, argument_parser_it>    m_subparser_map;                                               ///< Subparser map
    Map<String, bool>                                 m_subparser_used;                                              ///< Subparser used
    Vec<MutuallyExclusiveGroup>                       m_mutually_exclusive_groups;                                   ///< Mutually exclusive groups
    bool                                              m_suppress              = false;                               ///< Whether to suppress
    usize                                             m_usage_max_line_width  = (std::numeric_limits<usize>::max)(); ///< Maximum line width
    bool                                              m_usage_break_on_mutex  = false;                               ///< Whether to break on mutex
    int                                               m_usage_newline_counter = 0;                                   ///< Usage newline counter
    Vec<String>                                       m_group_names;                                                 ///< Group names
  };
} // namespace argparse

// NOLINTEND(readability-identifier-naming, readability-identifier-length, modernize-use-nullptr)
