#include <fmt/core.h>

#include "config.h"

using rfl::Result;

inline fn GetConfigPath() -> string {
  return getenv(
#ifdef __WIN32__
    "LOCALAPPDATA"
#else
    "HOME"
#endif
  );
}

fn Config::getInstance() -> Config {
  const string         path   = GetConfigPath() + "\\draconis++\\config.toml";
  const Result<Config> result = rfl::toml::load<Config>(path);

  if (result)
    return result.value();

  fmt::println("Failed to load config file: {}", result.error().value().what());
  return {};
}
