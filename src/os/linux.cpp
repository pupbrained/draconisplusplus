#ifdef __linux__

#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>

#include "os.h"

using std::string;

static const char *DBUS_INTERFACE         = "org.freedesktop.DBus",
                  *DBUS_OBJECT_PATH       = "/org/freedesktop/DBus",
                  *DBUS_METHOD_LIST_NAMES = "ListNames",
                  *MPRIS_INTERFACE_NAME   = "org.mpris.MediaPlayer2",
                  *PLAYER_OBJECT_PATH     = "/org/mpris/MediaPlayer2",
                  *PLAYER_INTERFACE_NAME  = "org.mpris.MediaPlayer2.Player";

u64 ParseLineAsNumber(const string& input) {
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

u64 MeminfoParse(std::ifstream input) {
  string line;

  // Skip every line before the one that starts with "MemTotal"
  while (std::getline(input, line) && !line.starts_with("MemTotal"));

  // Parse the number from the line
  const u64 num = ParseLineAsNumber(line);

  return num;
}

u64 GetMemInfo() {
  return MeminfoParse(std::ifstream("/proc/meminfo")) * 1024;
}

std::vector<std::string> GetMprisPlayers(sdbus::IConnection& connection) {
  std::unique_ptr<sdbus::IProxy> dbusProxy =
      sdbus::createProxy(connection, DBUS_INTERFACE, DBUS_OBJECT_PATH);

  std::vector<std::string> names;

  dbusProxy->callMethod(DBUS_METHOD_LIST_NAMES)
      .onInterface(DBUS_INTERFACE)
      .storeResultsTo(names);

  std::vector<std::string> mprisPlayers;

  for (const std::basic_string<char>& name : names)
    if (name.find(MPRIS_INTERFACE_NAME) != std::string::npos)
      mprisPlayers.push_back(name);

  return mprisPlayers;
}

std::string GetActivePlayer(const std::vector<std::string>& mprisPlayers) {
  if (!mprisPlayers.empty())
    return mprisPlayers.front();

  return "";
}

std::string GetNowPlaying() {
  try {
    std::unique_ptr<sdbus::IConnection> connection =
        sdbus::createSessionBusConnection();

    std::vector<std::string> mprisPlayers = GetMprisPlayers(*connection);

    if (mprisPlayers.empty())
      return "";

    std::string activePlayer = GetActivePlayer(mprisPlayers);

    if (activePlayer.empty())
      return "";

    std::unique_ptr<sdbus::IProxy> playerProxy =
        sdbus::createProxy(*connection, activePlayer, PLAYER_OBJECT_PATH);

    std::map<std::string, sdbus::Variant> metadata =
        playerProxy->getProperty("Metadata").onInterface(PLAYER_INTERFACE_NAME);

    auto iter = metadata.find("xesam:title");

    if (iter != metadata.end() &&
        iter->second.containsValueOfType<std::string>())
      return iter->second.get<std::string>();
  } catch (const sdbus::Error& e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return "";
}

#endif
