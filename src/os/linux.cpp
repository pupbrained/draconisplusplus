#ifdef __linux__

#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <sdbus-c++/sdbus-c++.h>
#include <sys/utsname.h>
#include <vector>

#include "os.h"
#include "src/util/macros.h"

enum SessionType : u8 { Wayland, X11, TTY, Unknown };

fn ParseLineAsNumber(const std::string& input) -> u64 {
  usize start = input.find_first_of("0123456789");

  if (start == std::string::npos) {
    ERROR_LOG("No number found in input");
    return 0;
  }

  usize end = input.find_first_not_of("0123456789", start);

  return std::stoull(input.substr(start, end - start));
}

fn MeminfoParse() -> u64 {
  constexpr const char* path = "/proc/meminfo";

  std::ifstream input(path);

  if (!input.is_open()) {
    ERROR_LOG("Failed to open {}", path);
    return 0;
  }

  std::string line;

  while (std::getline(input, line))
    if (line.starts_with("MemTotal"))
      return ParseLineAsNumber(line);

  ERROR_LOG("MemTotal line not found in {}", path);
  return 0;
}

fn GetMemInfo() -> u64 { return MeminfoParse() * 1024; }

fn GetOSVersion() -> std::string {
  constexpr const char* path = "/etc/os-release";

  std::ifstream file(path);

  if (!file.is_open()) {
    ERROR_LOG("Failed to open {}", path);
    return "";
  }

  string       line;
  const string prefix = "PRETTY_NAME=";

  while (std::getline(file, line))
    if (line.starts_with(prefix)) {
      string prettyName = line.substr(prefix.size());

      if (!prettyName.empty() && prettyName.front() == '"' && prettyName.back() == '"')
        return prettyName.substr(1, prettyName.size() - 2);

      return prettyName;
    }

  ERROR_LOG("PRETTY_NAME line not found in {}", path);
  return "";
}

fn GetMprisPlayers(sdbus::IConnection& connection) -> std::vector<string> {
  const sdbus::ServiceName dbusInterface       = sdbus::ServiceName("org.freedesktop.DBus");
  const sdbus::ObjectPath  dbusObjectPath      = sdbus::ObjectPath("/org/freedesktop/DBus");
  const char*              dbusMethodListNames = "ListNames";

  const std::unique_ptr<sdbus::IProxy> dbusProxy = createProxy(connection, dbusInterface, dbusObjectPath);

  std::vector<string> names;

  dbusProxy->callMethod(dbusMethodListNames).onInterface(dbusInterface).storeResultsTo(names);

  std::vector<string> mprisPlayers;

  for (const std::basic_string<char>& name : names)
    if (const char* mprisInterfaceName = "org.mpris.MediaPlayer2"; name.find(mprisInterfaceName) != std::string::npos)
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
    const char *playerObjectPath = "/org/mpris/MediaPlayer2", *playerInterfaceName = "org.mpris.MediaPlayer2.Player";

    std::unique_ptr<sdbus::IConnection> connection = sdbus::createSessionBusConnection();

    std::vector<string> mprisPlayers = GetMprisPlayers(*connection);

    if (mprisPlayers.empty()) {
      DEBUG_LOG("No MPRIS players found");
      return "";
    }

    string activePlayer = GetActivePlayer(mprisPlayers);

    if (activePlayer.empty()) {
      DEBUG_LOG("No active player found");
      return "";
    }

    std::unique_ptr<sdbus::IProxy> playerProxy =
      sdbus::createProxy(*connection, sdbus::ServiceName(activePlayer), sdbus::ObjectPath(playerObjectPath));

    sdbus::Variant metadataVariant = playerProxy->getProperty("Metadata").onInterface(playerInterfaceName);

    if (metadataVariant.containsValueOfType<std::map<std::string, sdbus::Variant>>()) {
      const std::map<std::basic_string<char>, sdbus::Variant>& metadata =
        metadataVariant.get<std::map<std::string, sdbus::Variant>>();

      auto iter = metadata.find("xesam:title");

      if (iter != metadata.end() && iter->second.containsValueOfType<std::string>())
        return iter->second.get<std::string>();
    }
  } catch (const sdbus::Error& e) {
    if (e.getName() != "com.github.altdesktop.playerctld.NoActivePlayer") {
      ERROR_LOG("Error: {}", e.what());
      return "";
    }

    return "No active player";
  }

  return "";
}

fn GetShell() -> string {
  const char* shell = std::getenv("SHELL");

  return shell ? shell : "";
}

fn GetHost() -> string {
  constexpr const char* path = "/sys/class/dmi/id/product_family";

  std::ifstream file(path);
  if (!file.is_open()) {
    ERROR_LOG("Failed to open {}", path);
    return "";
  }

  std::string productFamily;
  if (!std::getline(file, productFamily)) {
    ERROR_LOG("Failed to read from {}", path);
    return "";
  }

  return productFamily;
}

fn GetKernelVersion() -> string {
  struct utsname uts;

  if (uname(&uts) == -1) {
    ERROR_LOG("uname() failed: {}", std::strerror(errno));
    return "";
  }

  return static_cast<const char*>(uts.release);
}

#endif
