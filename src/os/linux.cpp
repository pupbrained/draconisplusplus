#ifdef __linux__

#include <cstring>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>

#include "os.h"

enum SessionType { Wayland, X11, TTY, Unknown };

fn ParseLineAsNumber(const std::string& input) -> u64 {
  usize start = input.find_first_of("0123456789");

  if (start == std::string::npos)
    throw std::runtime_error("No number found in input");

  usize end = input.find_first_not_of("0123456789", start);

  return std::stoull(input.substr(start, end - start));
}

fn MeminfoParse(const std::filesystem::path& filepath) -> u64 {
  std::ifstream input(filepath);

  if (!input.is_open())
    throw std::runtime_error("Failed to open " + filepath.string());

  std::string line;

  while (std::getline(input, line))
    if (line.starts_with("MemTotal"))
      return ParseLineAsNumber(line);

  throw std::runtime_error("MemTotal line not found in " + filepath.string());
}

fn GetMemInfo() -> u64 { return MeminfoParse("/proc/meminfo") * 1024; }

fn GetOSVersion() -> std::string {
  std::ifstream file("/etc/os-release");

  if (!file.is_open())
    throw std::runtime_error("Failed to open /etc/os-release");

  string       line;
  const string prefix = "PRETTY_NAME=";

  while (std::getline(file, line))
    if (line.starts_with(prefix)) {
      string prettyName = line.substr(prefix.size());

      if (!prettyName.empty() && prettyName.front() == '"' && prettyName.back() == '"')
        return prettyName.substr(1, prettyName.size() - 2);

      return prettyName;
    }

  throw std::runtime_error("PRETTY_NAME line not found in /etc/os-release");
}

fn GetMprisPlayers(sdbus::IConnection& connection) -> std::vector<string> {
  const sdbus::ServiceName dbusInterface       = sdbus::ServiceName("org.freedesktop.DBus");
  const sdbus::ObjectPath  dbusObjectPath      = sdbus::ObjectPath("/org/freedesktop/DBus");
  const char*              dbusMethodListNames = "ListNames";

  const std::unique_ptr<sdbus::IProxy> dbusProxy =
    createProxy(connection, dbusInterface, dbusObjectPath);

  std::vector<string> names;

  dbusProxy->callMethod(dbusMethodListNames).onInterface(dbusInterface).storeResultsTo(names);

  std::vector<string> mprisPlayers;

  for (const std::basic_string<char>& name : names)
    if (const char* mprisInterfaceName = "org.mpris.MediaPlayer2";
        name.find(mprisInterfaceName) != std::string::npos)
      mprisPlayers.push_back(name);

  return mprisPlayers;
}

fn GetActivePlayer(const std::vector<string>& mprisPlayers) -> string {
  if (!mprisPlayers.empty())
    return mprisPlayers.front();

  return "";
}

fn GetNowPlaying() -> string {
  try {
    const char *playerObjectPath    = "/org/mpris/MediaPlayer2",
               *playerInterfaceName = "org.mpris.MediaPlayer2.Player";

    std::unique_ptr<sdbus::IConnection> connection = sdbus::createSessionBusConnection();

    std::vector<string> mprisPlayers = GetMprisPlayers(*connection);

    if (mprisPlayers.empty())
      return "";

    string activePlayer = GetActivePlayer(mprisPlayers);

    if (activePlayer.empty())
      return "";

    auto playerProxy = sdbus::createProxy(
      *connection, sdbus::ServiceName(activePlayer), sdbus::ObjectPath(playerObjectPath)
    );

    sdbus::Variant metadataVariant =
      playerProxy->getProperty("Metadata").onInterface(playerInterfaceName);

    if (metadataVariant.containsValueOfType<std::map<std::string, sdbus::Variant>>()) {
      const auto& metadata = metadataVariant.get<std::map<std::string, sdbus::Variant>>();

      auto iter = metadata.find("xesam:title");

      if (iter != metadata.end() && iter->second.containsValueOfType<std::string>())
        return iter->second.get<std::string>();
    }
  } catch (const sdbus::Error& e) {
    if (e.getName() != "com.github.altdesktop.playerctld.NoActivePlayer")
      return fmt::format("Error: {}", e.what());

    return "No active player";
  }

  return "";
}

#endif
