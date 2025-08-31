/**
 * @file Localization.hpp
 * @brief Provides localization support for the Drac++ project.
 *
 * This header defines the TranslationManager class which handles loading
 * translation files and providing localized strings for the application.
 */

#pragma once

#include <Drac++/Utils/TranslationData.hpp>
#include <Drac++/Utils/Types.hpp>

namespace draconis::utils::localization {
  using namespace types;

  /**
   * @brief Represents a language with its code and display name.
   */
  struct Language {
    String code;        ///< Language code (e.g., "en", "es", "fr")
    String displayName; ///< Display name (e.g., "English", "Español")
    String fallback;    ///< Fallback language code (usually "en")
  };

  /**
   * @brief Manages translations for the application.
   *
   * The TranslationManager provides access to compile-time translation data
   * and supports switching between available languages with fallback to English.
   */
  class TranslationManager {
   public:
    /**
     * @brief Default constructor.
     */
    TranslationManager();

    /**
     * @brief Constructs a TranslationManager with a specific language.
     * @param languageCode The language code to load (e.g., "en", "es")
     */
    explicit TranslationManager(StringView languageCode);

    /**
     * @brief Sets the current language.
     * @param languageCode The language code to switch to
     * @return true if the language was successfully loaded, false otherwise
     */
    fn setLanguage(StringView languageCode) -> bool;

    /**
     * @brief Gets the current language code.
     * @return The current language code
     */
    fn getCurrentLanguage() const -> StringView;

    /**
     * @brief Gets a localized string by key.
     * @param key The translation key
     * @return The localized string, or the key itself if not found
     */
    fn get(StringView key) const -> String;

    /**
     * @brief Gets a localized string with fallback to English.
     * @param key The translation key
     * @return The localized string, or English translation, or key if not found
     */
    fn getWithFallback(StringView key) const -> String;

    /**
     * @brief Checks if a translation key exists.
     * @param key The translation key to check
     * @return true if the key exists in current language or fallback
     */
    fn hasKey(StringView key) const -> bool;

    /**
     * @brief Gets all available languages.
     * @return Vector of available Language objects
     */
    static fn getAvailableLanguages() -> Vec<Language>;

    /**
     * @brief Gets the system default language code.
     * @return The system language code, or "en" as fallback
     */
    static fn getSystemLanguage() -> String;

    /**
     * @brief Extracts language code from locale string (e.g., "en_US.UTF-8" -> "en").
     * @param localeStr The locale string to parse
     * @return The extracted language code
     */
    static fn extractLanguageCode(StringView localeStr) -> String;

   private:
    /**
     * @brief Loads translation data for a specific language.
     * @param languageCode The language code to load
     * @return true if successfully loaded, false otherwise
     */
    fn loadTranslations(StringView languageCode) -> bool;

    /**
     * @brief Computes hash for a string key (same as TranslationEntry::hash).
     * @param str The string to hash
     * @return The computed hash
     */
    static constexpr fn hashKey(StringView str) -> u64 {
      u64 hash = 0;

      for (char character : str)
        hash = (hash * 31) + static_cast<u8>(character);

      return hash;
    }

    String                                        m_currentLanguage;     ///< Current language code
    const std::array<data::TranslationEntry, 19>* m_englishTranslations; ///< English translations
    const std::array<data::TranslationEntry, 19>* m_currentTranslations; ///< Current language translations
    const data::TranslationMap<19>*               m_currentMap;          ///< Current language hash map for fast lookups
  };

  /**
   * @brief Global translation manager instance.
   */
  extern fn GetTranslationManager() -> TranslationManager&;

  /**
   * @brief Convenience function to get a localized string.
   * @param key The translation key
   * @return The localized string
   */
  inline fn _(StringView key) -> String {
    return GetTranslationManager().get(key);
  }

  /**
   * @brief Convenience function to get a localized string with fallback.
   * @param key The translation key
   * @return The localized string with fallback
   */
  inline fn _f(StringView key) -> String { // NOLINT(readability-identifier-naming)
    return GetTranslationManager().getWithFallback(key);
  }

  /**
   * @brief Convenience function to get a formatted localized string.
   * @tparam Args Format argument types
   * @param key The translation key
   * @param args Format arguments
   * @return The formatted localized string
   */
  template <typename... Args>
  inline fn _format(StringView key, Args&&... args) -> String { // NOLINT(readability-identifier-naming)
    return std::vformat(GetTranslationManager().get(key), std::make_format_args(std::forward<Args>(args)...));
  }

  /**
   * @brief Convenience function to get a formatted localized string with fallback.
   * @tparam Args Format argument types
   * @param key The translation key
   * @param args Format arguments
   * @return The formatted localized string with fallback
   */
  template <typename... Args>
  inline fn _format_f(StringView key, Args&&... args) -> String { // NOLINT(readability-identifier-naming)
    return std::vformat(GetTranslationManager().getWithFallback(key), std::make_format_args(std::forward<Args>(args)...));
  }
} // namespace draconis::utils::localization
