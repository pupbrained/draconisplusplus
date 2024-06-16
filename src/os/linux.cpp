#ifdef __linux__

#include <cstring>
#include <fstream>
#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>

#include "os.h"

using std::string;

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

fn GetOSVersion() -> const char* {
  std::ifstream file("/etc/os-release");

  if (!file.is_open()) {
    std::cerr << "Failed to open /etc/os-release" << std::endl;
    return nullptr;
  }

  std::string       line;
  const std::string prefix = "PRETTY_NAME=";

  while (std::getline(file, line)) {
    if (line.find(prefix) == 0) {
      std::string prettyName = line.substr(prefix.size());

      // Remove surrounding quotes if present
      if (!prettyName.empty() && prettyName.front() == '"' && prettyName.back() == '"')
        prettyName = prettyName.substr(1, prettyName.size() - 2);

      // Allocate memory for the C-string and copy the content
      char* cstr = new char[prettyName.size() + 1];
      std::strcpy(cstr, prettyName.c_str());

      return cstr;
    }
  }

  return nullptr;
}

fn GetMprisPlayers(sdbus::IConnection& connection) -> std::vector<std::string> {
  const char *dbusInterface = "org.freedesktop.DBus", *dbusObjectPath = "/org/freedesktop/DBus",
             *dbusMethodListNames = "ListNames";

  const std::unique_ptr<sdbus::IProxy> dbusProxy =
    createProxy(connection, dbusInterface, dbusObjectPath);

  std::vector<std::string> names;

  dbusProxy->callMethod(dbusMethodListNames).onInterface(dbusInterface).storeResultsTo(names);

  std::vector<std::string> mprisPlayers;

  for (const std::basic_string<char>& name : names)
    if (const char* mprisInterfaceName = "org.mpris.MediaPlayer2";
        name.contains(mprisInterfaceName))
      mprisPlayers.push_back(name);

  return mprisPlayers;
}

fn GetActivePlayer(const std::vector<std::string>& mprisPlayers) -> std::string {
  if (!mprisPlayers.empty())
    return mprisPlayers.front();

  return "";
}

fn GetNowPlaying() -> std::string {
  try {
    const char *playerObjectPath    = "/org/mpris/MediaPlayer2",
               *playerInterfaceName = "org.mpris.MediaPlayer2.Player";

    std::unique_ptr<sdbus::IConnection> connection = sdbus::createSessionBusConnection();

    std::vector<std::string> mprisPlayers = GetMprisPlayers(*connection);

    if (mprisPlayers.empty())
      return "";

    std::string activePlayer = GetActivePlayer(mprisPlayers);

    if (activePlayer.empty())
      return "";

    std::unique_ptr<sdbus::IProxy> playerProxy =
      sdbus::createProxy(*connection, activePlayer, playerObjectPath);

    std::map<std::string, sdbus::Variant> metadata =
      playerProxy->getProperty("Metadata").onInterface(playerInterfaceName);

    if (const auto iter = metadata.find("xesam:title");

        iter != metadata.end() && iter->second.containsValueOfType<std::string>())
      return iter->second.get<std::string>();
  } catch (const sdbus::Error& e) { std::cerr << "Error: " << e.what() << '\n'; }

  return "";
}

#endif
