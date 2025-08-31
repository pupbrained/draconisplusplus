#include <Drac++/Utils/Env.hpp>
#include <Drac++/Utils/Localization.hpp>
#include <Drac++/Utils/Logging.hpp>
#include <Drac++/Utils/TranslationData.hpp>

namespace draconis::utils::localization {
  namespace {
    // Helper function to find translation by hash (much faster than linear search)
    fn FindTranslation(u64 keyHash, const data::TranslationMap<19>& map) -> StringView {
      return map.find(keyHash);
    }

    // Fallback helper function using linear search (only when hash fails)
    fn FindTranslationLinear(StringView key, const Array<data::TranslationEntry, 19>& translations) -> String {
      for (const auto& entry : translations)
        if (entry.key == key)
          return String(entry.value);

      return String(key); // Return key if not found
    }

    // Helper function to find language info by code
    fn FindLanguageInfo(StringView code) -> const data::LanguageInfo* {
      for (const auto& lang : data::AVAILABLE_LANGUAGES)
        if (lang.code == code)
          return &lang;

      return nullptr;
    }
  } // namespace

  TranslationManager::TranslationManager()
    : m_currentLanguage(DRAC_DEFAULT_LANGUAGE),
      m_englishTranslations(&data::ENGLISH_TRANSLATIONS),
      m_currentTranslations(&data::ENGLISH_TRANSLATIONS),
      m_currentMap(&data::ENGLISH_MAP) {
    // Try to load default language first, then system language, then fallback to English
    bool languageSet = false;

    // Try default language from build system
    if (String(DRAC_DEFAULT_LANGUAGE) != "en")
      languageSet = setLanguage(DRAC_DEFAULT_LANGUAGE);

    // If default language didn't work or is English, try system language
    if (!languageSet)
      if (String systemLang = getSystemLanguage(); systemLang != "en" && setLanguage(systemLang))
        languageSet = true;

    // Final fallback to English
    if (!languageSet)
      setLanguage("en");
  }

  TranslationManager::TranslationManager(StringView languageCode)
    : m_currentLanguage(languageCode),
      m_englishTranslations(&data::ENGLISH_TRANSLATIONS),
      m_currentTranslations(&data::ENGLISH_TRANSLATIONS),
      m_currentMap(&data::ENGLISH_MAP) {
    // Load English fallback first
    loadTranslations("en");

    // Load requested language
    if (!loadTranslations(languageCode))
      m_currentLanguage = "en";
  }

  fn TranslationManager::setLanguage(StringView languageCode) -> bool {
    if (languageCode == m_currentLanguage)
      return true;

    debug_log("Attempting to set language to: {}", languageCode);

    if (loadTranslations(languageCode)) {
      m_currentLanguage = languageCode;
      debug_log("Successfully set language to: {}", m_currentLanguage);
      return true;
    }

    debug_log("Failed to load translations for language: {}", languageCode);
    return false;
  }

  fn TranslationManager::getCurrentLanguage() const -> StringView {
    return m_currentLanguage;
  }

  fn TranslationManager::get(StringView key) const -> String {
    // Use hash-based lookup for O(1) performance (much faster than linear search)
    if (m_currentMap) {
      uint64_t         keyHash = hashKey(key);
      std::string_view result  = FindTranslation(keyHash, *m_currentMap);
      if (!result.empty()) { // Found translation
        debug_log("Translation found for key '{}' in current language: '{}'", key, String(result));
        return String(result);
      }
    }

    // Fallback to linear search if hash lookup fails (should rarely happen)
    if (m_currentTranslations) {
      String result = FindTranslationLinear(key, *m_currentTranslations);
      if (result != String(key)) { // Found translation
        debug_log("Translation found (linear) for key '{}' in current language: '{}'", key, result);
        return result;
      }
    }

    debug_log("Translation NOT found for key '{}' in current language, returning key as-is", key);
    return String(key); // Return key if not found
  }

  fn TranslationManager::getWithFallback(StringView key) const -> String {
    // get() already handles current language, so just return that
    String result = get(key);
    if (result != String(key)) {
      return result;
    }

    debug_log("Translation NOT found for key '{}' anywhere, returning key as-is", key);
    return String(key); // Return key if not found anywhere
  }

  fn TranslationManager::hasKey(StringView key) const -> bool {
    // Use hash-based lookup for O(1) performance
    if (m_currentMap) {
      uint64_t         keyHash = hashKey(key);
      std::string_view result  = FindTranslation(keyHash, *m_currentMap);
      return !result.empty();
    }

    // Fallback to linear search
    if (m_currentTranslations) {
      String result = FindTranslationLinear(key, *m_currentTranslations);
      return result != String(key);
    }

    return false;
  }

  fn TranslationManager::getAvailableLanguages() -> Vec<Language> {
    Vec<Language> languages;
    languages.reserve(data::AVAILABLE_LANGUAGES.size()); // Avoid reallocations
    for (const auto& lang : data::AVAILABLE_LANGUAGES) {
      languages.emplace_back(
        String(lang.code), String(lang.displayName),
        "en" // All fallback to English
      );
    }
    return languages;
  }

  fn TranslationManager::getSystemLanguage() -> String {
    // Try to get from environment variables
    auto langEnv = env::GetEnv("LANG");
    if (langEnv) {
      // Extract language code from LANG (e.g., "en_US.UTF-8" -> "en")
      return extractLanguageCode(*langEnv);
    }

    auto lcAllEnv = env::GetEnv("LC_ALL");
    if (lcAllEnv) {
      return extractLanguageCode(*lcAllEnv);
    }

    // Fallback to English
    return "en";
  }

  // Helper function to extract language code from locale string
  fn TranslationManager::extractLanguageCode(StringView localeStr) -> String {
    size_t     dotPos   = localeStr.find('.');
    StringView langPart = localeStr.substr(0, dotPos);

    size_t underscorePos = langPart.find('_');
    return String(langPart.substr(0, underscorePos));
  }

  fn TranslationManager::loadTranslations(StringView languageCode) -> bool {
    const data::LanguageInfo* langInfo = FindLanguageInfo(languageCode);

    if (!langInfo) {
      debug_log("Language '{}' not found in available languages", languageCode);
      return false;
    }

    debug_log("Loading translations for language: {}", languageCode);

    if (languageCode == "en") {
      m_englishTranslations = &data::ENGLISH_TRANSLATIONS;
      m_currentTranslations = &data::ENGLISH_TRANSLATIONS; // For English, both point to the same
      m_currentMap          = &data::ENGLISH_MAP;
      debug_log("Loaded English translations");
    } else {
      m_currentTranslations = langInfo->translations;
      m_currentMap          = langInfo->map;
      debug_log("Loaded {} translations for language '{}'", langInfo->translations->size(), languageCode);
    }

    return true;
  }

  fn GetTranslationManager() -> TranslationManager& {
    static TranslationManager GlobalTranslationManager;
    return GlobalTranslationManager;
  }
} // namespace draconis::utils::localization
