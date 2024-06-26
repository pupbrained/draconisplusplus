#include <fmt/core.h>

#include "config.h"

using rfl::Result;

fn Config::getInstance() -> Config {
#ifdef __WIN32__
  const string path = string(getenv("LOCALAPPDATA")) + "\\draconis++\\config.toml";
#else
  const string path = string(getenv("HOME")) + "/.config/draconis++/config.toml";
#endif
  const Result<Config> result = rfl::toml::load<Config>(path);

  if (result)
    return result.value();

  fmt::println("Failed to load config file: {}", result.error().value().what());
  return {};
}

