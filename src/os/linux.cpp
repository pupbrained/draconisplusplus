#ifdef __linux__

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fmt/format.h>
#include <fstream>
#include <ranges>
#include <sdbus-c++/sdbus-c++.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <vector>
#include <wayland-client.h>

#include "os.h"
#include "src/util/macros.h"

enum SessionType : u8 { Wayland, X11, TTY, Unknown };

namespace {
  fn MeminfoParse() -> u64 {
    constexpr const char* path = "/proc/meminfo";

    std::ifstream input(path);
    if (!input.is_open()) {
      ERROR_LOG("Failed to open {}", path);
      return 0;
    }

    std::string line;
    while (std::getline(input, line)) {
      if (line.starts_with("MemTotal")) {
        const size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) {
          ERROR_LOG("Invalid MemTotal line: no colon found");
          return 0;
        }

        std::string_view view(line);
        view.remove_prefix(colonPos + 1);

        // Trim leading whitespace
        const size_t firstNonSpace = view.find_first_not_of(' ');
        if (firstNonSpace == std::string_view::npos) {
          ERROR_LOG("No number found after colon in MemTotal line");
          return 0;
        }
        view.remove_prefix(firstNonSpace);

        // Find the end of the numeric part
        const size_t end = view.find_first_not_of("0123456789");
        if (end != std::string_view::npos) {
          view = view.substr(0, end);
        }

        // Get pointers via iterators
        const char* startPtr = &*view.begin(); // Safe iterator-to-pointer conversion
        const char* endPtr   = &*view.end();   // No manual arithmetic

        u64        value  = 0;
        const auto result = std::from_chars(startPtr, endPtr, value);
        if (result.ec != std::errc() || result.ptr != endPtr) {
          ERROR_LOG("Failed to parse number in MemTotal line");
          return 0;
        }

        return value;
      }
    }

    ERROR_LOG("MemTotal line not found in {}", path);
    return 0;
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

  fn GetX11WindowManager() -> string {
    Display* display = XOpenDisplay(nullptr);
    if (!display)
      return "Unknown (X11)";

    Atom supportingWmCheck = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    Atom wmName            = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8String        = XInternAtom(display, "UTF8_STRING", False);

    // ignore unsafe buffer access warning, can't really get around it
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
    Window root = DefaultRootWindow(display);
#pragma clang diagnostic pop

    Window         wmWindow     = 0;
    Atom           actualType   = 0;
    int            actualFormat = 0;
    unsigned long  nitems = 0, bytesAfter = 0;
    unsigned char* data = nullptr;

    if (XGetWindowProperty(
          display,
          root,
          supportingWmCheck,
          0,
          1,
          False,
          XA_WINDOW,
          &actualType,
          &actualFormat,
          &nitems,
          &bytesAfter,
          &data
        ) == Success &&
        data) {
      wmWindow = *std::bit_cast<Window*>(data);
      XFree(data);
      data = nullptr;

      if (XGetWindowProperty(
            display,
            wmWindow,
            wmName,
            0,
            1024,
            False,
            utf8String,
            &actualType,
            &actualFormat,
            &nitems,
            &bytesAfter,
            &data
          ) == Success &&
          data) {
        std::string name(std::bit_cast<char*>(data));
        XFree(data);
        XCloseDisplay(display);
        return name;
      }
    }

    XCloseDisplay(display);

    return "Unknown (X11)";
  }

  fn TrimHyprlandWrapper(const std::string& input) -> std::string {
    if (input.find("hyprland") != std::string::npos)
      return "Hyprland";
    return input;
  }

  fn ReadProcessCmdline(int pid) -> std::string {
    std::string   path = "/proc/" + std::to_string(pid) + "/cmdline";
    std::ifstream cmdlineFile(path);
    std::string   cmdline;
    if (std::getline(cmdlineFile, cmdline)) {
      // Replace null bytes with spaces
      std::ranges::replace(cmdline, '\0', ' ');
      return cmdline;
    }
    return "";
  }

  fn DetectHyprlandSpecific() -> std::string {
    // Check environment variables first
    const char* xdgCurrentDesktop = std::getenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop && strcasestr(xdgCurrentDesktop, "hyprland")) {
      return "Hyprland";
    }

    // Check for Hyprland's specific environment variable
    if (std::getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
      return "Hyprland";
    }

    // Check for Hyprland socket
    std::string socketPath = "/run/user/" + std::to_string(getuid()) + "/hypr";
    if (std::filesystem::exists(socketPath)) {
      return "Hyprland";
    }

    return "";
  }

  fn GetWaylandCompositor() -> std::string {
    // First try Hyprland-specific detection
    std::string hypr = DetectHyprlandSpecific();
    if (!hypr.empty())
      return hypr;

    // Then try the standard Wayland detection
    wl_display* display = wl_display_connect(nullptr);
    if (!display)
      return "";

    int fileDescriptor = wl_display_get_fd(display);

    struct ucred cred;
    socklen_t    len = sizeof(cred);
    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
      wl_display_disconnect(display);
      return "";
    }

    // Read both comm and cmdline
    std::string compositorName;

    // 1. Check comm (might be wrapped)
    std::string   commPath = "/proc/" + std::to_string(cred.pid) + "/comm";
    std::ifstream commFile(commPath);
    if (commFile >> compositorName) {
      std::ranges::subrange removedRange = std::ranges::remove(compositorName, '\n');
      compositorName.erase(removedRange.begin(), compositorName.end());
    }

    // 2. Check cmdline for actual binary reference
    std::string cmdline = ReadProcessCmdline(cred.pid);
    if (cmdline.find("hyprland") != std::string::npos) {
      wl_display_disconnect(display);
      return "Hyprland";
    }

    // 3. Check exe symlink
    std::string                exePath = "/proc/" + std::to_string(cred.pid) + "/exe";
    std::array<char, PATH_MAX> buf;
    ssize_t                    lenBuf = readlink(exePath.c_str(), buf.data(), buf.size() - 1);
    if (lenBuf != -1) {
      buf.at(static_cast<usize>(lenBuf)) = '\0';
      std::string exe(buf.data());
      if (exe.find("hyprland") != std::string::npos) {
        wl_display_disconnect(display);
        return "Hyprland";
      }
    }

    wl_display_disconnect(display);

    // Final cleanup of wrapper names
    return TrimHyprlandWrapper(compositorName);
  }

  // Helper functions
  fn ToLowercase(std::string str) -> std::string {
    std::ranges::transform(str, str.begin(), ::tolower);
    return str;
  }

  fn ContainsAny(std::string_view haystack, const std::vector<std::string>& needles) -> bool {
    return std::ranges::any_of(needles, [&](auto& n) { return haystack.find(n) != std::string_view::npos; });
  }

  fn DetectFromEnvVars() -> std::string {
    // Check XDG_CURRENT_DESKTOP
    if (const char* xdgDe = std::getenv("XDG_CURRENT_DESKTOP")) {
      std::string_view sview(xdgDe);
      if (!sview.empty()) {
        std::string deStr(sview);
        if (size_t colon = deStr.find(':'); colon != std::string::npos)
          deStr.erase(colon);
        if (!deStr.empty())
          return deStr;
      }
    }

    // Check DESKTOP_SESSION
    if (const char* desktopSession = std::getenv("DESKTOP_SESSION")) {
      std::string_view sview(desktopSession);
      if (!sview.empty())
        return std::string(sview);
    }

    return "";
  }

  fn DetectFromSessionFiles() -> std::string {
    namespace fs                             = std::filesystem;
    const std::vector<fs::path> sessionPaths = { "/usr/share/xsessions", "/usr/share/wayland-sessions" };

    const std::vector<std::pair<std::string, std::vector<std::string>>> dePatterns = {
      {      "KDE",           { "plasma", "plasmax11", "kde" } },
      {    "GNOME", { "gnome", "gnome-xorg", "gnome-wayland" } },
      {     "XFCE",                                 { "xfce" } },
      {     "MATE",                                 { "mate" } },
      { "Cinnamon",                             { "cinnamon" } },
      {   "Budgie",                               { "budgie" } },
      {     "LXQt",                                 { "lxqt" } },
      {    "Unity",                                { "unity" } }
    };

    for (const auto& sessionPath : sessionPaths) {
      if (!fs::exists(sessionPath))
        continue;

      for (const auto& entry : fs::directory_iterator(sessionPath)) {
        if (!entry.is_regular_file())
          continue;

        const std::string filename      = entry.path().stem();
        auto              lowerFilename = ToLowercase(filename);

        for (const auto& [deName, patterns] : dePatterns) {
          if (ContainsAny(lowerFilename, patterns))
            return deName;
        }
      }
    }
    return "";
  }

  fn DetectFromProcesses() -> std::string {
    const std::vector<std::pair<std::string, std::string>> processChecks = {
      {     "plasmashell",      "KDE" },
      {     "gnome-shell",    "GNOME" },
      {   "xfce4-session",     "XFCE" },
      {    "mate-session",     "MATE" },
      { "cinnamon-sessio", "Cinnamon" },
      {       "budgie-wm",   "Budgie" },
      {    "lxqt-session",     "LXQt" }
    };

    std::ifstream cmdline("/proc/self/environ");
    std::string   envVars((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());

    for (const auto& [process, deName] : processChecks) {
      if (envVars.find(process) != std::string::npos)
        return deName;
    }

    return "Unknown";
  }

}

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

fn GetMemInfo() -> u64 { return MeminfoParse() * 1024; }

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

fn GetWindowManager() -> string {
  // Check environment variables first
  const char* xdgSessionType = std::getenv("XDG_SESSION_TYPE");
  const char* waylandDisplay = std::getenv("WAYLAND_DISPLAY");

  // Prefer Wayland detection if Wayland session
  if ((waylandDisplay != nullptr) || (xdgSessionType && strstr(xdgSessionType, "wayland"))) {
    std::string compositor = GetWaylandCompositor();
    if (!compositor.empty())
      return compositor;

    // Fallback environment check
    const char* xdgCurrentDesktop = std::getenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop) {
      std::string desktop(xdgCurrentDesktop);
      std::ranges::transform(compositor, compositor.begin(), ::tolower);
      if (desktop.find("hyprland") != std::string::npos)
        return "hyprland";
    }
  }

  // X11 detection
  std::string x11wm = GetX11WindowManager();
  if (!x11wm.empty())
    return x11wm;

  return "Unknown";
}

fn GetDesktopEnvironment() -> string {
  // Try environment variables first
  if (auto desktopEnvironment = DetectFromEnvVars(); !desktopEnvironment.empty())
    return desktopEnvironment;

  // Try session files next
  if (auto desktopEnvironment = DetectFromSessionFiles(); !desktopEnvironment.empty())
    return desktopEnvironment;

  // Fallback to process detection
  return DetectFromProcesses();
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
