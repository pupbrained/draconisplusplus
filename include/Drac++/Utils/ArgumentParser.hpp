/**
 * @file ArgumentParser.hpp
 * @brief Simple command-line argument parser for Drac++.
 *
 * This header provides a lightweight argument parser that follows the Drac++
 * coding conventions and type system. It supports basic argument parsing
 * including flags, optional arguments, and help text generation.
 */

#pragma once

#include <algorithm>
#include <concepts>                  // std::convertible_to
#include <cstdlib>                   // std::exit
#include <format>                    // std::format
#include <magic_enum/magic_enum.hpp> // magic_enum::enum_name, magic_enum::enum_cast
#include <sstream>                   // std::ostringstream
#include <utility>                   // std::forward

#include "Error.hpp"
#include "Logging.hpp"
#include "Types.hpp"

namespace draconis::utils::argparse {
  namespace {
    using error::DracError;
    using error::DracErrorCode;
    using logging::Print;
    using logging::Println;

    using types::Err;
    using types::f64;
    using types::i32;
    using types::Map;
    using types::Option;
    using types::Result;
    using types::Span;
    using types::String;
    using types::StringView;
    using types::UniquePointer;
    using types::Unit;
    using types::usize;
    using types::Vec;
  } // namespace

  /**
   * @brief Type alias for argument values.
   */
  using ArgValue = std::variant<bool, i32, f64, String>;

  /**
   * @brief Type alias for allowed choices for enum-style arguments.
   */
  using ArgChoices = Vec<String>;

  /**
   * @brief Generic traits class for enum string conversion using magic_enum.
   * @tparam EnumType The enum type
   */
  template <typename EnumType>
  struct EnumTraits {
    static constexpr bool has_string_conversion = magic_enum::is_scoped_enum_v<EnumType>;

    static fn getChoices() -> ArgChoices {
      static_assert(has_string_conversion, "Enum type must be a scoped enum");

      ArgChoices choices;
      const auto enumValues = magic_enum::enum_values<EnumType>();
      choices.reserve(enumValues.size());

      for (const auto value : enumValues)
        choices.emplace_back(magic_enum::enum_name(value));

      return choices;
    }

    static fn stringToEnum(const String& str) -> EnumType {
      static_assert(has_string_conversion, "Enum type must be a scoped enum");

      auto result = magic_enum::enum_cast<EnumType>(str);
      if (result.has_value())
        return result.value();

      const auto enumValues = magic_enum::enum_values<EnumType>();
      for (const auto value : enumValues) {
        StringView enumName = magic_enum::enum_name(value);
        if (std::ranges::equal(str, enumName, [](char charA, char charB) { return std::tolower(charA) == std::tolower(charB); }))
          return value;
      }

      return enumValues[0];
    }

    static fn enumToString(EnumType value) -> String {
      static_assert(has_string_conversion, "Enum type must be a scoped enum");
      return String(magic_enum::enum_name(value));
    }
  };

  /**
   * @brief Represents a command-line argument with its metadata and value.
   */
  class Argument {
   public:
    /**
     * @brief Construct a new Argument.
     * @param names Vector of argument names (e.g., {"-v", "--verbose"})
     * @param help_text Help text for this argument
     * @param is_flag Whether this is a flag (boolean) argument
     */
    explicit Argument(Vec<String> names, String help_text = "", bool is_flag = false)
      : m_names(std::move(names)), m_helpText(std::move(help_text)), m_isFlag(is_flag) {
      if (m_isFlag)
        m_defaultValue = false;
    }

    /**
     * @brief Construct a new Argument with variadic names.
     * @tparam NameTs Variadic list of types convertible to String
     * @param help_text Help text for this argument
     * @param is_flag Whether this is a flag (boolean) argument
     * @param names One or more argument names
     */
    template <typename... NameTs>
      requires(sizeof...(NameTs) >= 1 && (std::convertible_to<NameTs, String> && ...))
    explicit Argument(String help_text, bool is_flag, NameTs&&... names)
      : m_names { String(std::forward<NameTs>(names))... }, m_helpText(std::move(help_text)), m_isFlag(is_flag) {
      if (m_isFlag)
        m_defaultValue = false;
    }

    /**
     * @brief Set the help text for this argument.
     * @param help_text The help text
     * @return Reference to this argument for method chaining
     */
    fn help(String help_text) -> Argument& {
      m_helpText = std::move(help_text);
      return *this;
    }

    /**
     * @brief Set the default value for this argument.
     * @tparam T Type of the default value
     * @param value The default value
     * @return Reference to this argument for method chaining
     */
    template <typename T>
    fn defaultValue(T value) -> Argument& {
      m_defaultValue = std::move(value);
      return *this;
    }

    /**
     * @brief Set the default value for this argument as an enum.
     * @tparam EnumType The enum type
     * @param value The default enum value
     * @return Reference to this argument for method chaining
     */
    template <typename EnumType>
      requires std::is_enum_v<EnumType> && EnumTraits<EnumType>::has_string_conversion
                                         fn defaultValue(EnumType value) -> Argument& {
      String strValue = EnumTraits<EnumType>::enumToString(value);

      m_defaultValue = strValue;

      m_choices = EnumTraits<EnumType>::getChoices();

      return *this;
    }

    /**
     * @brief Configure this argument as a flag.
     * @return Reference to this argument for method chaining
     */
    fn flag() -> Argument& {
      m_isFlag       = true;
      m_defaultValue = false;
      return *this;
    }

    /**
     * @brief Set allowed choices for enum-style arguments.
     * @param choices Vector of allowed string values
     * @return Reference to this argument for method chaining
     */
    fn choices(ArgChoices choices) -> Argument& {
      m_choices = std::move(choices);
      return *this;
    }

    /**
     * @brief Get the value of this argument.
     * @tparam T Type to get the value as
     * @return The argument value, or default value if not provided
     */
    template <typename T>
    fn get() const -> T {
      if (m_isUsed && m_value.has_value())
        return std::get<T>(m_value.value());

      if (m_defaultValue.has_value())
        return std::get<T>(m_defaultValue.value());

      return T {};
    }

    /**
     * @brief Get the value of this argument as an enum type.
     * @tparam EnumType The enum type to convert to
     * @return The argument value converted to the enum type
     */
    template <typename EnumType>
      requires std::is_enum_v<EnumType> && EnumTraits<EnumType>::has_string_conversion
                                         fn getEnum() const -> EnumType {
      const auto strValue = get<String>();

      return EnumTraits<EnumType>::stringToEnum(strValue);
    }

    /**
     * @brief Check if this argument was used in the command line.
     * @return true if the argument was used, false otherwise
     */
    [[nodiscard]] fn isUsed() const -> bool {
      return m_isUsed;
    }

    /**
     * @brief Get the primary name of this argument.
     * @return The first name in the names list
     */
    [[nodiscard]] fn getPrimaryName() const -> const String& {
      return m_names.front();
    }

    /**
     * @brief Get all names for this argument.
     * @return Vector of all argument names
     */
    [[nodiscard]] fn getNames() const -> const Vec<String>& {
      return m_names;
    }

    /**
     * @brief Get the help text for this argument.
     * @return The help text
     */
    [[nodiscard]] fn getHelpText() const -> const String& {
      return m_helpText;
    }

    /**
     * @brief Check if this argument is a flag.
     * @return true if this is a flag argument, false otherwise
     */
    [[nodiscard]] fn isFlag() const -> bool {
      return m_isFlag;
    }

    /**
     * @brief Check if this argument has choices (enum-style).
     * @return true if this argument has choices, false otherwise
     */
    [[nodiscard]] fn hasChoices() const -> bool {
      return m_choices.has_value();
    }

    /**
     * @brief Get the allowed choices for this argument.
     * @return Vector of allowed choices, or empty vector if none set
     */
    [[nodiscard]] fn getChoices() const -> ArgChoices {
      return m_choices.value_or(ArgChoices {});
    }

    /**
     * @brief Set the value for this argument.
     * @param value The value to set
     * @return Result indicating success or failure
     */
    fn setValue(ArgValue value) -> Result<> {
      if (hasChoices() && std::holds_alternative<String>(value)) {
        const String&     strValue = std::get<String>(value);
        const ArgChoices& choices  = m_choices.value();

        bool isValid = false;
        for (const String& choice : choices) {
          if (std::ranges::equal(strValue, choice, [](char charA, char charB) { return std::tolower(charA) == std::tolower(charB); })) {
            isValid = true;
            break;
          }
        }

        if (!isValid) {
          std::ostringstream choicesStream;
          for (usize i = 0; i < choices.size(); ++i) {
            if (i > 0)
              choicesStream << ", ";
            String lower = choices[i];
            std::ranges::transform(lower, lower.begin(), [](char character) { return std::tolower(character); });
            choicesStream << lower;
          }

          return Err(DracError(
            DracErrorCode::InvalidArgument,
            std::format("Invalid value '{}' for argument '{}'. Allowed values: {}", strValue, getPrimaryName(), choicesStream.str())
          ));
        }
      }

      m_value  = std::move(value);
      m_isUsed = true;
      return {};
    }

    /**
     * @brief Mark this argument as used.
     */
    fn markUsed() -> Unit {
      m_isUsed = true;

      if (m_isFlag)
        m_value = true;
    }

   private:
    Vec<String>        m_names;        ///< Argument names (e.g., {"-v", "--verbose"})
    String             m_helpText;     ///< Help text for this argument
    Option<ArgValue>   m_value;        ///< The actual value provided
    Option<ArgValue>   m_defaultValue; ///< Default value if none provided
    Option<ArgChoices> m_choices;      ///< Allowed choices for enum-style arguments
    bool               m_isFlag {};    ///< Whether this is a flag argument
    bool               m_isUsed {};    ///< Whether this argument was used
  };

  /**
   * @brief Main argument parser class.
   */
  class ArgumentParser {
   public:
    /**
     * @brief Construct a new ArgumentParser.
     * @param programName Name of the program (for help messages)
     * @param version Version string of the program
     */
    explicit ArgumentParser(String programName = "", String version = "1.0")
      : m_programName(std::move(programName)), m_version(std::move(version)) {
      addArguments("-h", "--help")
        .help("Show this help message and exit")
        .flag()
        .defaultValue(false);

      addArguments("-v", "--version")
        .help("Show version information and exit")
        .flag()
        .defaultValue(false);
    }

    /**
     * @brief Add a new argument (or multiple aliases) to the parser.
     *
     * This variadic overload allows callers to pass one or more names directly, e.g.
     *   parser.addArgument("-f", "--file");
     * without the need to manually construct a `Vec<String>` or invoke `addArguments`.
     *
     * @tparam NameTs Variadic list of types convertible to `String`
     * @param names   One or more argument names / aliases
     * @return Reference to the newly created argument
     */
    template <typename... NameTs>
      requires(sizeof...(NameTs) >= 1 && (std::convertible_to<NameTs, String> && ...))
    fn addArguments(NameTs&&... names) -> Argument& {
      m_arguments.emplace_back(std::make_unique<Argument>(String {}, false, std::forward<NameTs>(names)...));
      Argument& arg = *m_arguments.back();

      for (const String& name : arg.getNames())
        m_argumentMap[name] = &arg;

      return arg;
    }

    /**
     * @brief Parse command-line arguments.
     * @param args Span of argument strings
     * @return Result indicating success or failure
     */
    fn parseArgs(Span<const char* const> args) -> Result<> {
      Vec<String> stringArgs;
      stringArgs.reserve(args.size());

      for (const char* arg : args)
        stringArgs.emplace_back(arg);

      return parseArgs(stringArgs);
    }

    /**
     * @brief Parse command-line arguments from a vector.
     * @param args Vector of argument strings
     * @return Result indicating success or failure
     */
    fn parseArgs(const Vec<String>& args) -> Result<> {
      if (args.empty())
        return {};

      if (m_programName.empty())
        m_programName = args[0];

      for (usize i = 1; i < args.size(); ++i) {
        const String& arg = args[i];

        if (arg == "-h" || arg == "--help") {
          printHelp();
          std::exit(0);
        }

        if (arg == "-v" || arg == "--version") {
          Println(m_version);
          std::exit(0);
        }

        auto iter = m_argumentMap.find(arg);
        if (iter == m_argumentMap.end())
          return Err(DracError(DracErrorCode::InvalidArgument, std::format("Unknown argument: {}", arg)));

        Argument* argument = iter->second;

        if (argument->isFlag()) {
          argument->markUsed();
        } else {
          if (i + 1 >= args.size())
            return Err(DracError(DracErrorCode::InvalidArgument, std::format("Argument {} requires a value", arg)));

          String value = args[++i];
          if (Result result = argument->setValue(value); !result) {
            return result;
          }
        }
      }

      return {};
    }

    /**
     * @brief Get the value of an argument.
     * @tparam T Type to get the value as
     * @param name Argument name
     * @return The argument value, or default value if not provided
     */
    template <typename T = String>
    fn get(StringView name) const -> T {
      auto iter = m_argumentMap.find(name);

      if (iter != m_argumentMap.end())
        return iter->second->get<T>();

      return T {};
    }

    /**
     * @brief Get the value of an argument as an enum type.
     * @tparam EnumType The enum type to convert to
     * @param name Argument name
     * @return The argument value converted to the enum type
     */
    template <typename EnumType>
    fn getEnum(StringView name) const -> EnumType {
      auto iter = m_argumentMap.find(name);

      if (iter != m_argumentMap.end())
        return iter->second->getEnum<EnumType>();

      static_assert(EnumTraits<EnumType>::has_string_conversion, "Enum type not supported. Add a specialization to EnumTraits.");
      return EnumTraits<EnumType>::stringToEnum("");
    }

    /**
     * @brief Check if an argument was used.
     * @param name Argument name
     * @return true if the argument was used, false otherwise
     */
    [[nodiscard]] fn isUsed(StringView name) const -> bool {
      auto iter = m_argumentMap.find(name);
      if (iter != m_argumentMap.end())
        return iter->second->isUsed();

      return false;
    }

    /**
     * @brief Print help message.
     */
    fn printHelp() const -> Unit {
      std::ostringstream usageStream;
      usageStream << "Usage: " << m_programName;

      for (const auto& arg : m_arguments)
        if (arg->getPrimaryName().starts_with('-')) {
          usageStream << " [" << arg->getPrimaryName();

          if (!arg->isFlag())
            usageStream << " VALUE";

          usageStream << "]";
        }

      Println(usageStream.str());
      Println();

      if (!m_arguments.empty()) {
        Println("Arguments:");
        for (const auto& arg : m_arguments) {
          std::ostringstream namesStream;
          for (usize i = 0; i < arg->getNames().size(); ++i) {
            if (i > 0)
              namesStream << ", ";

            namesStream << arg->getNames()[i];
          }

          std::ostringstream argLineStream;
          argLineStream << "  " << namesStream.str();
          if (!arg->isFlag())
            argLineStream << " VALUE";

          Println(argLineStream.str());

          if (!arg->getHelpText().empty())
            Println("    " + arg->getHelpText());

          if (arg->hasChoices()) {
            std::ostringstream choicesStream;
            choicesStream << "    Available values: ";
            const ArgChoices& choices = arg->getChoices();
            for (usize i = 0; i < choices.size(); ++i) {
              if (i > 0)
                choicesStream << ", ";

              String lower = choices[i];

              std::ranges::transform(lower, lower.begin(), [](char character) { return std::tolower(character); });

              choicesStream << lower;
            }
            Println(choicesStream.str());
          }

          Println();
        }
      }
    }

   private:
    String                       m_programName; ///< Program name
    String                       m_version;     ///< Program version
    Vec<UniquePointer<Argument>> m_arguments;   ///< List of all arguments
    Map<String, Argument*>       m_argumentMap; ///< Map of argument names to arguments
  };
} // namespace draconis::utils::argparse