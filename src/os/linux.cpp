#ifdef __linux__

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <cstring>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <sdbus-c++/sdbus-c++.h>
#include <vector>

#include "os.h"

enum SessionType { Wayland, X11, TTY, Unknown };

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

  return ParseLineAsNumber(line);
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

fn GetDesktopEnvironment() -> string {
  const char* xdgCurrentDesktop = std::getenv("XDG_CURRENT_DESKTOP");

  if (xdgCurrentDesktop)
    return xdgCurrentDesktop;

  return std::getenv("DESKTOP_SESSION");
}

fn GetSessionType() -> SessionType {
  string xdgSessionType = std::getenv("XDG_SESSION_TYPE");

  if (xdgSessionType == "wayland")
    return Wayland;
  if (xdgSessionType == "x11")
    return X11;
  if (xdgSessionType == "tty")
    return TTY;
  return Unknown;
}

fn Exec(const char* cmd) -> string {
  std::array<char, 128>                    buffer;
  std::string                              result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }

  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) { result += buffer.data(); }

  return result;
}

fn GetWindowManager() -> string {
  string xdgSessionType = std::getenv("XDG_SESSION_TYPE");

  if (xdgSessionType == "wayland") {
    // TODO implement wayland window manager
  }

  if (xdgSessionType == "x11") {
    Display* display = XOpenDisplay(nullptr);

    Atom wmCheckAtom = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", True);
    if (wmCheckAtom == None) {
      return "Unknown (no _NET_SUPPORTING_WM_CHECK)";
    }

    Window         root = DefaultRootWindow(display);
    Atom           actualType = 0;
    int            actualFormat = 0;
    unsigned long  itemCount = 0, bytesAfter = 0;
    unsigned char* data = nullptr;

    int status = XGetWindowProperty(
      display,
      root,
      wmCheckAtom,
      0,
      1,
      False,
      XA_WINDOW,
      &actualType,
      &actualFormat,
      &itemCount,
      &bytesAfter,
      &data
    );
    if (status != Success || !data) {
      return "Unknown (failed to get _NET_SUPPORTING_WM_CHECK)";
    }

    Window wmWindow = *(Window*)(data);
    XFree(data);

    status = XGetWindowProperty(
      display,
      wmWindow,
      wmCheckAtom,
      0,
      1,
      False,
      XA_WINDOW,
      &actualType,
      &actualFormat,
      &itemCount,
      &bytesAfter,
      &data
    );
    if (status != Success || !data) {
      return "Unknown (failed to get supporting window)";
    }

    wmWindow = *(reinterpret_cast<Window*>(data));
    XFree(data);

    Atom wmNameAtom = XInternAtom(display, "_NET_WM_NAME", True);
    if (wmNameAtom == None) {
      return "Unknown (no _NET_WM_NAME)";
    }

    status = XGetWindowProperty(
      display,
      wmWindow,
      wmNameAtom,
      0,
      (~0L),
      False,
      AnyPropertyType,
      &actualType,
      &actualFormat,
      &itemCount,
      &bytesAfter,
      &data
    );
    if (status != Success || !data) {
      return "Unknown (failed to get _NET_WM_NAME)";
    }

    std::string wmName(reinterpret_cast<char*>(data));
    XFree(data);

    return wmName;
  }

  if (xdgSessionType == "tty")
    return "TTY";

  return "Unknown";
}

#endif
