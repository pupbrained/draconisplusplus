#ifdef __linux__

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstring>
#include <dbus/dbus.h>
#include <dirent.h>
#include <expected>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <ranges>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <vector>
#include <wayland-client.h>

#include "os.h"
#include "src/util/macros.h"

// Minimal global using declarations needed for function signatures
using std::expected;
using std::optional;

namespace fs = std::filesystem;
using namespace std::literals::string_view_literals;

namespace {
  // Local using declarations for the anonymous namespace
  using std::array;
  using std::bit_cast;
  using std::getenv;
  using std::ifstream;
  using std::istreambuf_iterator;
  using std::less;
  using std::lock_guard;
  using std::mutex;
  using std::nullopt;
  using std::ofstream;
  using std::pair;
  using std::string_view;
  using std::to_string;
  using std::unexpected;
  using std::vector;
  using std::ranges::is_sorted;
  using std::ranges::lower_bound;
  using std::ranges::replace;
  using std::ranges::subrange;
  using std::ranges::transform;

  fn GetX11WindowManager() -> string {
    Display* display = XOpenDisplay(nullptr);

    // If XOpenDisplay fails, likely in a TTY
    if (!display)
      return "";

    Atom supportingWmCheck = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
    Atom wmName            = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8String        = XInternAtom(display, "UTF8_STRING", False);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
    Window root = DefaultRootWindow(display); // NOLINT
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
          // XA_WINDOW
          static_cast<Atom>(33),
          &actualType,
          &actualFormat,
          &nitems,
          &bytesAfter,
          &data
        ) == Success &&
        data) {
      wmWindow = *bit_cast<Window*>(data);
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
        string name(bit_cast<char*>(data));
        XFree(data);
        XCloseDisplay(display);
        return name;
      }
    }

    XCloseDisplay(display);

    return "Unknown (X11)"; // Changed to empty string
  }

  fn TrimHyprlandWrapper(const string& input) -> string {
    if (input.contains("hyprland"))
      return "Hyprland";
    return input;
  }

  fn ReadProcessCmdline(int pid) -> string {
    string   path = "/proc/" + to_string(pid) + "/cmdline";
    ifstream cmdlineFile(path);
    string   cmdline;
    if (getline(cmdlineFile, cmdline)) {
      // Replace null bytes with spaces
      replace(cmdline, '\0', ' ');
      return cmdline;
    }
    return "";
  }

  fn DetectHyprlandSpecific() -> string {
    // Check environment variables first
    const char* xdgCurrentDesktop = getenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop && strcasestr(xdgCurrentDesktop, "hyprland"))
      return "Hyprland";

    // Check for Hyprland's specific environment variable
    if (getenv("HYPRLAND_INSTANCE_SIGNATURE"))
      return "Hyprland";

    // Check for Hyprland socket
    if (fs::exists("/run/user/" + to_string(getuid()) + "/hypr"))
      return "Hyprland";

    return "";
  }

  fn GetWaylandCompositor() -> string {
    // First try Hyprland-specific detection
    string hypr = DetectHyprlandSpecific();
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
    string compositorName;

    // 1. Check comm (might be wrapped)
    string   commPath = "/proc/" + to_string(cred.pid) + "/comm";
    ifstream commFile(commPath);
    if (commFile >> compositorName) {
      subrange removedRange = std::ranges::remove(compositorName, '\n');
      compositorName.erase(removedRange.begin(), compositorName.end());
    }

    // 2. Check cmdline for actual binary reference
    string cmdline = ReadProcessCmdline(cred.pid);
    if (cmdline.contains("hyprland")) {
      wl_display_disconnect(display);
      return "Hyprland";
    }

    // 3. Check exe symlink
    string                exePath = "/proc/" + to_string(cred.pid) + "/exe";
    array<char, PATH_MAX> buf;
    ssize_t               lenBuf = readlink(exePath.c_str(), buf.data(), buf.size() - 1);
    if (lenBuf != -1) {
      buf.at(static_cast<usize>(lenBuf)) = '\0';
      string exe(buf.data());
      if (exe.contains("hyprland")) {
        wl_display_disconnect(display);
        return "Hyprland";
      }
    }

    wl_display_disconnect(display);

    // Final cleanup of wrapper names
    return TrimHyprlandWrapper(compositorName);
  }

  fn DetectFromEnvVars() -> optional<string> {
    // Use RAII to guard against concurrent env modifications
    static mutex      EnvMutex;
    lock_guard<mutex> lock(EnvMutex);

    // XDG_CURRENT_DESKTOP
    if (const char* xdgCurrentDesktop = getenv("XDG_CURRENT_DESKTOP")) {
      const string_view sview(xdgCurrentDesktop);
      const size_t      colon = sview.find(':');
      return string(sview.substr(0, colon)); // Direct construct from view
    }

    // DESKTOP_SESSION
    if (const char* desktopSession = getenv("DESKTOP_SESSION"))
      return string(string_view(desktopSession)); // Avoid intermediate view storage

    return nullopt;
  }

  fn DetectFromSessionFiles() -> optional<string> {
    static constexpr array<pair<string_view, string_view>, 12> DE_PATTERNS = {
      // clang-format off
      pair {        "Budgie"sv,   "budgie"sv },
      pair {      "Cinnamon"sv, "cinnamon"sv },
      pair {          "LXQt"sv,     "lxqt"sv },
      pair {          "MATE"sv,     "mate"sv },
      pair {         "Unity"sv,    "unity"sv },
      pair {         "gnome"sv,    "GNOME"sv },
      pair { "gnome-wayland"sv,    "GNOME"sv },
      pair {    "gnome-xorg"sv,    "GNOME"sv },
      pair {           "kde"sv,      "KDE"sv },
      pair {        "plasma"sv,      "KDE"sv },
      pair {     "plasmax11"sv,      "KDE"sv },
      pair {          "xfce"sv,     "XFCE"sv },
      // clang-format on
    };

    static_assert(is_sorted(DE_PATTERNS, {}, &pair<string_view, string_view>::first));

    // Precomputed session paths
    static constexpr array<string_view, 2> SESSION_PATHS = { "/usr/share/xsessions", "/usr/share/wayland-sessions" };

    // Single memory reserve for lowercase conversions
    string lowercaseStem;
    lowercaseStem.reserve(32);

    for (const auto& path : SESSION_PATHS) {
      if (!fs::exists(path))
        continue;

      for (const auto& entry : fs::directory_iterator(path)) {
        if (!entry.is_regular_file())
          continue;

        // Reuse buffer
        lowercaseStem = entry.path().stem().string();
        transform(lowercaseStem, lowercaseStem.begin(), ::tolower);

        // Modern ranges version
        const pair<string_view, string_view>* const patternIter = lower_bound(
          DE_PATTERNS, lowercaseStem, less {}, &pair<string_view, string_view>::first // Projection
        );

        if (patternIter != DE_PATTERNS.end() && patternIter->first == lowercaseStem)
          return string(patternIter->second);
      }
    }

    return nullopt;
  }

  fn DetectFromProcesses() -> optional<string> {
    const array processChecks = {
      // clang-format off
      pair {     "plasmashell"sv,      "KDE"sv },
      pair {     "gnome-shell"sv,    "GNOME"sv },
      pair {   "xfce4-session"sv,     "XFCE"sv },
      pair {    "mate-session"sv,     "MATE"sv },
      pair { "cinnamon-sessio"sv, "Cinnamon"sv },
      pair {       "budgie-wm"sv,   "Budgie"sv },
      pair {    "lxqt-session"sv,     "LXQt"sv },
      // clang-format on
    };

    ifstream cmdline("/proc/self/environ");
    string   envVars((istreambuf_iterator<char>(cmdline)), istreambuf_iterator<char>());

    for (const auto& [process, deName] : processChecks)
      if (envVars.contains(process))
        return string(deName);

    return nullopt;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
  fn GetMprisPlayers(DBusConnection* connection) -> vector<string> {
    vector<string> mprisPlayers;
    DBusError      err;
    dbus_error_init(&err);

    // Create a method call to org.freedesktop.DBus.ListNames
    DBusMessage* msg = dbus_message_new_method_call(
      "org.freedesktop.DBus",  // target service
      "/org/freedesktop/DBus", // object path
      "org.freedesktop.DBus",  // interface name
      "ListNames"              // method name
    );

    if (!msg) {
      DEBUG_LOG("Failed to create message for ListNames.");
      return mprisPlayers;
    }

    // Send the message and block until we get a reply.
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
      DEBUG_LOG("DBus error in ListNames: {}", err.message);
      dbus_error_free(&err);
      return mprisPlayers;
    }

    if (!reply) {
      DEBUG_LOG("No reply received for ListNames.");
      return mprisPlayers;
    }

    // The expected reply signature is "as" (an array of strings)
    DBusMessageIter iter;

    if (!dbus_message_iter_init(reply, &iter)) {
      DEBUG_LOG("Reply has no arguments.");
      dbus_message_unref(reply);
      return mprisPlayers;
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
      DEBUG_LOG("Reply argument is not an array.");
      dbus_message_unref(reply);
      return mprisPlayers;
    }

    // Iterate over the array of strings
    DBusMessageIter subIter;
    dbus_message_iter_recurse(&iter, &subIter);

    while (dbus_message_iter_get_arg_type(&subIter) != DBUS_TYPE_INVALID) {
      if (dbus_message_iter_get_arg_type(&subIter) == DBUS_TYPE_STRING) {
        const char* name = nullptr;
        dbus_message_iter_get_basic(&subIter, static_cast<void*>(&name));
        if (name && std::string_view(name).contains("org.mpris.MediaPlayer2"))
          mprisPlayers.emplace_back(name);
      }
      dbus_message_iter_next(&subIter);
    }

    dbus_message_unref(reply);
    return mprisPlayers;
  }
#pragma clang diagnostic pop

  fn GetActivePlayer(const vector<string>& mprisPlayers) -> optional<string> {
    if (!mprisPlayers.empty())
      return mprisPlayers.front();
    return nullopt;
  }
}

fn GetOSVersion() -> expected<string, string> {
  constexpr const char* path = "/etc/os-release";

  ifstream file(path);

  if (!file.is_open())
    return unexpected("Failed to open " + string(path));

  string       line;
  const string prefix = "PRETTY_NAME=";

  while (getline(file, line))
    if (line.starts_with(prefix)) {
      string prettyName = line.substr(prefix.size());

      if (!prettyName.empty() && prettyName.front() == '"' && prettyName.back() == '"')
        return prettyName.substr(1, prettyName.size() - 2);

      return prettyName;
    }

  return unexpected("PRETTY_NAME line not found in " + string(path));
}

fn GetMemInfo() -> expected<u64, string> {
  using std::from_chars, std::errc;

  constexpr const char* path = "/proc/meminfo";

  ifstream input(path);

  if (!input.is_open())
    return unexpected("Failed to open " + string(path));

  string line;
  while (getline(input, line)) {
    if (line.starts_with("MemTotal")) {
      const size_t colonPos = line.find(':');

      if (colonPos == string::npos)
        return unexpected("Invalid MemTotal line: no colon found");

      string_view view(line);
      view.remove_prefix(colonPos + 1);

      // Trim leading whitespace
      const size_t firstNonSpace = view.find_first_not_of(' ');

      if (firstNonSpace == string_view::npos)
        return unexpected("No number found after colon in MemTotal line");

      view.remove_prefix(firstNonSpace);

      // Find the end of the numeric part
      const size_t end = view.find_first_not_of("0123456789");
      if (end != string_view::npos)
        view = view.substr(0, end);

      // Get pointers via iterators
      const char* startPtr = &*view.begin();
      const char* endPtr   = &*view.end();

      u64        value  = 0;
      const auto result = from_chars(startPtr, endPtr, value);

      if (result.ec != errc() || result.ptr != endPtr)
        return unexpected("Failed to parse number in MemTotal line");

      return value * 1024;
    }
  }

  return unexpected("MemTotal line not found in " + string(path));
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"
fn GetNowPlaying() -> expected<string, NowPlayingError> {
  DBusError err;
  dbus_error_init(&err);

  // Connect to the session bus
  DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, &err);

  if (!connection)
    if (dbus_error_is_set(&err)) {
      ERROR_LOG("DBus connection error: {}", err.message);

      NowPlayingError error = LinuxError(err.message);
      dbus_error_free(&err);

      return unexpected(error);
    }

  vector<string> mprisPlayers = GetMprisPlayers(connection);

  if (mprisPlayers.empty()) {
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { NowPlayingCode::NoPlayers });
  }

  optional<string> activePlayer = GetActivePlayer(mprisPlayers);

  if (!activePlayer.has_value()) {
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { NowPlayingCode::NoActivePlayer });
  }

  // Prepare a call to the Properties.Get method to fetch "Metadata"
  DBusMessage* msg = dbus_message_new_method_call(
    activePlayer->c_str(),             // target service (active player)
    "/org/mpris/MediaPlayer2",         // object path
    "org.freedesktop.DBus.Properties", // interface
    "Get"                              // method name
  );

  if (!msg) {
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { /* error creating message */ });
  }

  const char* interfaceName = "org.mpris.MediaPlayer2.Player";
  const char* propertyName  = "Metadata";

  if (!dbus_message_append_args(
        msg, DBUS_TYPE_STRING, &interfaceName, DBUS_TYPE_STRING, &propertyName, DBUS_TYPE_INVALID
      )) {
    dbus_message_unref(msg);
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { /* error appending arguments */ });
  }

  // Call the method and block until reply is received.
  DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, msg, -1, &err);
  dbus_message_unref(msg);

  if (dbus_error_is_set(&err)) {
    ERROR_LOG("DBus error in Properties.Get: {}", err.message);

    NowPlayingError error = LinuxError(err.message);
    dbus_error_free(&err);
    dbus_connection_unref(connection);

    return unexpected(error);
  }

  if (!reply) {
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { /* no reply error */ });
  }

  // The reply should contain a variant holding a dictionary ("a{sv}")
  DBusMessageIter iter;

  if (!dbus_message_iter_init(reply, &iter)) {
    dbus_message_unref(reply);
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { /* no arguments in reply */ });
  }

  if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
    dbus_message_unref(reply);
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { /* unexpected argument type */ });
  }

  // Recurse into the variant to get the dictionary
  DBusMessageIter variantIter;
  dbus_message_iter_recurse(&iter, &variantIter);

  if (dbus_message_iter_get_arg_type(&variantIter) != DBUS_TYPE_ARRAY) {
    dbus_message_unref(reply);
    dbus_connection_unref(connection);
    return unexpected(NowPlayingError { /* expected array type */ });
  }

  string          title;
  string          artist;
  DBusMessageIter arrayIter;
  dbus_message_iter_recurse(&variantIter, &arrayIter);

  // Iterate over each dictionary entry (each entry is of type dict entry)
  while (dbus_message_iter_get_arg_type(&arrayIter) != DBUS_TYPE_INVALID) {
    if (dbus_message_iter_get_arg_type(&arrayIter) == DBUS_TYPE_DICT_ENTRY) {
      DBusMessageIter dictEntry;
      dbus_message_iter_recurse(&arrayIter, &dictEntry);

      // Get the key (a string)
      const char* key = nullptr;

      if (dbus_message_iter_get_arg_type(&dictEntry) == DBUS_TYPE_STRING)
        dbus_message_iter_get_basic(&dictEntry, static_cast<void*>(&key));

      // Move to the value (a variant)
      dbus_message_iter_next(&dictEntry);

      if (dbus_message_iter_get_arg_type(&dictEntry) == DBUS_TYPE_VARIANT) {
        DBusMessageIter valueIter;
        dbus_message_iter_recurse(&dictEntry, &valueIter);

        if (key && std::string_view(key) == "xesam:title") {
          if (dbus_message_iter_get_arg_type(&valueIter) == DBUS_TYPE_STRING) {
            const char* val = nullptr;
            dbus_message_iter_get_basic(&valueIter, static_cast<void*>(&val));

            if (val)
              title = val;
          }
        } else if (key && std::string_view(key) == "xesam:artist") {
          // Expect an array of strings
          if (dbus_message_iter_get_arg_type(&valueIter) == DBUS_TYPE_ARRAY) {
            DBusMessageIter subIter;
            dbus_message_iter_recurse(&valueIter, &subIter);

            if (dbus_message_iter_get_arg_type(&subIter) == DBUS_TYPE_STRING) {
              const char* val = nullptr;
              dbus_message_iter_get_basic(&subIter, static_cast<void*>(&val));

              if (val)
                artist = val;
            }
          }
        }
      }
    }

    dbus_message_iter_next(&arrayIter);
  }

  dbus_message_unref(reply);
  dbus_connection_unref(connection);

  string result;

  if (!artist.empty() && !title.empty())
    result = artist + " - " + title;
  else if (!title.empty())
    result = title;
  else if (!artist.empty())
    result = artist;
  else
    result = "";

  return result;
}
#pragma clang diagnostic pop

fn GetWindowManager() -> string {
  // Check environment variables first
  const char* xdgSessionType = getenv("XDG_SESSION_TYPE");
  const char* waylandDisplay = getenv("WAYLAND_DISPLAY");

  // Prefer Wayland detection if Wayland session
  if ((waylandDisplay != nullptr) || (xdgSessionType && string_view(xdgSessionType).contains("wayland"))) {
    string compositor = GetWaylandCompositor();
    if (!compositor.empty())
      return compositor;

    // Fallback environment check
    const char* xdgCurrentDesktop = getenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop) {
      string desktop(xdgCurrentDesktop);
      transform(compositor, compositor.begin(), ::tolower);
      if (desktop.contains("hyprland"))
        return "hyprland";
    }
  }

  // X11 detection
  string x11wm = GetX11WindowManager();
  if (!x11wm.empty())
    return x11wm;

  return "Unknown";
}

fn GetDesktopEnvironment() -> optional<string> {
  // Try environment variables first
  if (auto desktopEnvironment = DetectFromEnvVars(); desktopEnvironment.has_value())
    return desktopEnvironment;

  // Try session files next
  if (auto desktopEnvironment = DetectFromSessionFiles(); desktopEnvironment.has_value())
    return desktopEnvironment;

  // Fallback to process detection
  return DetectFromProcesses();
}

fn GetShell() -> string {
  const string_view shell = getenv("SHELL");

  if (shell.ends_with("bash"))
    return "Bash";
  if (shell.ends_with("zsh"))
    return "Zsh";
  if (shell.ends_with("fish"))
    return "Fish";
  if (shell.ends_with("nu"))
    return "Nushell";
  if (shell.ends_with("sh"))
    return "SH";

  return !shell.empty() ? string(shell) : "";
}

fn GetHost() -> string {
  constexpr const char* path = "/sys/class/dmi/id/product_family";

  ifstream file(path);
  if (!file.is_open()) {
    ERROR_LOG("Failed to open {}", path);
    return "";
  }

  string productFamily;
  if (!getline(file, productFamily)) {
    ERROR_LOG("Failed to read from {}", path);
    return "";
  }

  return productFamily;
}

fn GetKernelVersion() -> string {
  struct utsname uts;

  if (uname(&uts) == -1) {
    ERROR_LOG("uname() failed: {}", strerror(errno));
    return "";
  }

  return static_cast<const char*>(uts.release);
}

fn GetDiskUsage() -> pair<u64, u64> {
  struct statvfs stat;
  if (statvfs("/", &stat) == -1) {
    ERROR_LOG("statvfs() failed: {}", strerror(errno));
    return { 0, 0 };
  }
  return { (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize), stat.f_blocks * stat.f_frsize };
}

#endif
