/**
 * @file TranslationData.hpp
 * @brief Compile-time translation data for Drac++.
 *
 * This header contains all translation strings as compile-time constants
 * to avoid file I/O and ensure translations are always available.
 */

#pragma once

#include <Drac++/Utils/Types.hpp>

namespace draconis::utils::localization::data {
  namespace {
    using namespace std::string_view_literals;

    using utils::types::Array;
    using utils::types::Pair;
    using utils::types::StringView;
    using utils::types::u64;
    using utils::types::u8;
    using utils::types::usize;
  } // namespace

  /**
   * @brief Translation entry structure.
   */
  struct TranslationEntry {
    StringView key;
    StringView value;

    constexpr TranslationEntry(StringView key, StringView value)
      : key(key), value(value) {}

    // Compile-time hash for key
    static constexpr fn hash(StringView str) -> u64 {
      u64 hash = 0;

      for (char character : str)
        hash = (hash * 31) + static_cast<u8>(character);

      return hash;
    }

    [[nodiscard]] constexpr fn keyHash() const -> u64 {
      return hash(key);
    }
  };

  // clang-format off
  constexpr Array<TranslationEntry, 19> ENGLISH_TRANSLATIONS = {{
    {      "hello", "Hello {0}!" },
    {       "date",       "Date" },
    {    "weather",    "Weather" },
    {       "host",       "Host" },
    {         "os",         "OS" },
    {     "kernel",     "Kernel" },
    {        "ram",        "RAM" },
    {       "disk",       "Disk" },
    {        "cpu",        "CPU" },
    {        "gpu",        "GPU" },
    {     "uptime",     "Uptime" },
    {      "shell",      "Shell" },
    {   "packages",   "Packages" },
    {         "wm",         "WM" },
    {         "de",         "DE" },
    {    "playing",   "Playing " },
    {    "celsius",          "C" },
    { "fahrenheit",          "F" },
    {    "unknown",    "Unknown" },
  }};
  // clang-format on

  // clang-format off
  constexpr Array<TranslationEntry, 19> SPANISH_TRANSLATIONS = {{
    {      "hello",          "¡Hola {0}!" },
    {       "date",               "Fecha" },
    {    "weather",               "Clima" },
    {       "host",                "Host" },
    {         "os",                  "SO" },
    {     "kernel",              "Kernel" },
    {        "ram",                 "RAM" },
    {       "disk",               "Disco" },
    {        "cpu",                 "CPU" },
    {        "gpu",                 "GPU" },
    {     "uptime", "Tiempo de actividad" },
    {      "shell",               "Shell" },
    {   "packages",            "Paquetes" },
    {         "wm",                  "WM" },
    {         "de",                  "DE" },
    {    "playing",      "Reproduciendo " },
    {    "celsius",                   "C" },
    { "fahrenheit",                   "F" },
    {    "unknown",         "Desconocido" },
  }};
  // clang-format on

  // clang-format off
  constexpr Array<TranslationEntry, 19> FRENCH_TRANSLATIONS = {{
    {      "hello",     "Bonjour {0}!" },
    {       "date",             "Date" },
    {    "weather",            "Météo" },
    {       "host",             "Hôte" },
    {         "os",               "OS" },
    {     "kernel",            "Noyau" },
    {        "ram",              "RAM" },
    {       "disk",           "Disque" },
    {        "cpu",              "CPU" },
    {        "gpu",              "GPU" },
    {     "uptime", "Temps d'activité" },
    {      "shell",            "Shell" },
    {   "packages",          "Paquets" },
    {         "wm",               "WM" },
    {         "de",               "DE" },
    {    "playing",         "Lecture " },
    {    "celsius",                "C" },
    { "fahrenheit",                "F" },
    {    "unknown",          "Inconnu" },
  }};
  // clang-format on

  // clang-format off
  constexpr Array<TranslationEntry, 19> GERMAN_TRANSLATIONS = {{
    {      "hello",   "Hallo {0}!" },
    {       "date",        "Datum" },
    {    "weather",       "Wetter" },
    {       "host",         "Host" },
    {         "os",           "OS" },
    {     "kernel",       "Kernel" },
    {        "ram",          "RAM" },
    {       "disk",   "Festplatte" },
    {        "cpu",          "CPU" },
    {        "gpu",          "GPU" },
    {     "uptime", "Betriebszeit" },
    {      "shell",        "Shell" },
    {   "packages",       "Pakete" },
    {         "wm",           "WM" },
    {         "de",           "DE" },
    {    "playing",  "Wiedergabe " },
    {    "celsius",            "C" },
    { "fahrenheit",            "F" },
    {    "unknown",    "Unbekannt" },
  }};
  // clang-format on

  /**
   * @brief Simple compile-time hash map for fast translation lookups.
   */
  template <usize n>
  struct TranslationMap {
    Array<Pair<u64, StringView>, n> entries;

    [[nodiscard]] constexpr fn find(u64 hash) const -> StringView {
      for (const auto& entry : entries)
        if (entry.first == hash)
          return entry.second;

      return ""sv; // Not found
    }
  };

  /**
   * @brief Helper to create translation map from array.
   */
  template <usize n>
  constexpr fn CreateTranslationMap(const Array<TranslationEntry, n>& translations) -> TranslationMap<n> {
    TranslationMap<n> map {};

    for (usize i = 0; i < n; ++i)
      map.entries.at(i) = { translations.at(i).keyHash(), translations.at(i).value };

    return map;
  }

  /**
   * @brief English translation map for O(1) lookups.
   */
  constexpr TranslationMap<19> ENGLISH_MAP = CreateTranslationMap(ENGLISH_TRANSLATIONS);

  /**
   * @brief Spanish translation map for O(1) lookups.
   */
  constexpr TranslationMap<19> SPANISH_MAP = CreateTranslationMap(SPANISH_TRANSLATIONS);

  /**
   * @brief French translation map for O(1) lookups.
   */
  constexpr TranslationMap<19> FRENCH_MAP = CreateTranslationMap(FRENCH_TRANSLATIONS);

  /**
   * @brief German translation map for O(1) lookups.
   */
  constexpr TranslationMap<19> GERMAN_MAP = CreateTranslationMap(GERMAN_TRANSLATIONS);

  /**
   * @brief Language information structure with hash map.
   */
  struct LanguageInfo {
    StringView                         code;
    StringView                         displayName;
    const Array<TranslationEntry, 19>* translations;
    const TranslationMap<19>*          map;

    constexpr LanguageInfo(StringView code, StringView displayName, const Array<TranslationEntry, 19>* translations, const TranslationMap<19>* map)
      : code(code), displayName(displayName), translations(translations), map(map) {}
  };

  // clang-format off
  constexpr Array<LanguageInfo, 4> AVAILABLE_LANGUAGES = {{
    { "en",  "English", &ENGLISH_TRANSLATIONS, &ENGLISH_MAP },
    { "es",  "Español", &SPANISH_TRANSLATIONS, &SPANISH_MAP },
    { "fr", "Français",  &FRENCH_TRANSLATIONS,  &FRENCH_MAP },
    { "de",  "Deutsch",  &GERMAN_TRANSLATIONS,  &GERMAN_MAP },
  }};
  // clang-format on
} // namespace draconis::utils::localization::data
