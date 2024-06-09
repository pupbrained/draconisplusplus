#ifdef __linux__

#include <cstring>
#include <fstream>
#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>

#include "os.h"

using std::string;

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

u64 GetMemInfo() { return MeminfoParse(std::ifstream("/proc/meminfo")) * 1024; }

const char* GetOSVersion() {
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
      // clang-format off
      if (!prettyName.empty() &&
           prettyName.front() == '"' &&
           prettyName.back()  == '"')
        prettyName = prettyName.substr(1, prettyName.size() - 2);
      // clang-format on

      // Allocate memory for the C-string and copy the content
      char* cstr = new char[prettyName.size() + 1];
      std::strcpy(cstr, prettyName.c_str());

      return cstr;
    }
  }

  return nullptr;
}

std::vector<std::string> GetMprisPlayers(sdbus::IConnection& connection) {
  const char *dbusInterface       = "org.freedesktop.DBus",
             *dbusObjectPath      = "/org/freedesktop/DBus",
             *dbusMethodListNames = "ListNames",
             *mprisInterfaceName  = "org.mpris.MediaPlayer2";

  std::unique_ptr<sdbus::IProxy> dbusProxy =
      sdbus::createProxy(connection, dbusInterface, dbusObjectPath);

  std::vector<std::string> names;

  dbusProxy->callMethod(dbusMethodListNames)
      .onInterface(dbusInterface)
      .storeResultsTo(names);

  std::vector<std::string> mprisPlayers;

  for (const std::basic_string<char>& name : names)
    if (name.contains(mprisInterfaceName)) mprisPlayers.push_back(name);

  return mprisPlayers;
}

std::string GetActivePlayer(const std::vector<std::string>& mprisPlayers) {
  if (!mprisPlayers.empty()) return mprisPlayers.front();

  return "";
}

std::string GetNowPlaying() {
  try {
    const char *playerObjectPath    = "/org/mpris/MediaPlayer2",
               *playerInterfaceName = "org.mpris.MediaPlayer2.Player";

    std::unique_ptr<sdbus::IConnection> connection =
        sdbus::createSessionBusConnection();

    std::vector<std::string> mprisPlayers = GetMprisPlayers(*connection);

    if (mprisPlayers.empty()) return "";

    std::string activePlayer = GetActivePlayer(mprisPlayers);

    if (activePlayer.empty()) return "";

    std::unique_ptr<sdbus::IProxy> playerProxy =
        sdbus::createProxy(*connection, activePlayer, playerObjectPath);

    std::map<std::string, sdbus::Variant> metadata =
        playerProxy->getProperty("Metadata").onInterface(playerInterfaceName);

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
