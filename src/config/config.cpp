#include "config.h"

fn Config::getInstance() -> const Config& {
  static const Config* INSTANCE = new Config(rfl::toml::load<Config>("./config.toml").value());
  return *INSTANCE;
}
