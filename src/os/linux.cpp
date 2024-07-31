#ifdef __linux__

#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>

#include "os.h"

fn ParseLineAsNumber(const string& input) -> u64 {
  // Find the first number
  string::size_type start = 0;

  // Skip leading non-numbers
  while (!isdigit(input[++start]));

  // Start searching from the start of the number
  string::size_type end = start;

  // Increment to the end of the number
  while (isdigit(input[++end]));

  // Return the substring containing the number
  return std::stoul(input.substr(start, end - start));
}

fn MeminfoParse(std::ifstream input) -> u64 {
  string line;

  // Skip every line before the one that starts with "MemTotal"
  while (std::getline(input, line) && !line.starts_with("MemTotal"));

  // Parse the number from the line
  const u64 num = ParseLineAsNumber(line);

  return num;
}

fn GetMemInfo() -> u64 { return MeminfoParse(std::ifstream("/proc/meminfo")) * 1024; }

fn GetOSVersion() -> string {
  std::ifstream file("/etc/os-release");

  if (!file.is_open()) {
    std::cerr << "Failed to open /etc/os-release" << std::endl;
    return ""; // Return empty string indicating failure
  }

  string       line;
  const string prefix = "PRETTY_NAME=";

  while (std::getline(file, line)) {
    if (line.find(prefix) == 0) {
      string prettyName = line.substr(prefix.size());

      // Remove surrounding quotes if present
      if (!prettyName.empty() && prettyName.front() == '"' && prettyName.back() == '"')
        prettyName = prettyName.substr(1, prettyName.size() - 2);

      return prettyName;
    }
  }

  return ""; // Return empty string if PRETTY_NAME= line not found
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
