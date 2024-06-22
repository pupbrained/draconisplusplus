#include "config.h"

fn Config::getInstance()->const Config& {
#ifdef __WIN32__
  const string path = string(getenv("LOCALAPPDATA")) + "\\draconis++\\config.toml";
#else
  const string path = string(getenv("HOME")) + "/.config/draconis++/config.toml";
#endif
  // ReSharper disable once CppDFAMemoryLeak
  static const Config* INSTANCE = new Config(rfl::toml::load<Config>(path).value());
  return *INSTANCE;
}
