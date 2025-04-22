#ifdef __linux__

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
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

#ifdef Success
#undef Success
#endif
#ifdef None
#undef None
#endif

#include <dbus-cxx.h>

using std::expected;
using std::optional;

namespace fs = std::filesystem;
using namespace std::literals::string_view_literals;

namespace {
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

  fn GetX11WindowManager() -> String {
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
        ) == 0 &&
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
          ) == 0 &&
          data) {
        String name(bit_cast<char*>(data));
        XFree(data);
        XCloseDisplay(display);
        return name;
      }
    }

    XCloseDisplay(display);

    return "Unknown (X11)";
  }

  fn TrimHyprlandWrapper(const String& input) -> String {
    if (input.contains("hyprland"))
      return "Hyprland";
    return input;
  }

  fn ReadProcessCmdline(int pid) -> String {
    String   path = "/proc/" + to_string(pid) + "/cmdline";
    ifstream cmdlineFile(path);
    String   cmdline;
    if (getline(cmdlineFile, cmdline)) {
      // Replace null bytes with spaces
      replace(cmdline, '\0', ' ');
      return cmdline;
    }
    return "";
  }

  fn DetectHyprlandSpecific() -> String {
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

  fn GetWaylandCompositor() -> String {
    // First try Hyprland-specific detection
    String hypr = DetectHyprlandSpecific();
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
    String compositorName;

    // 1. Check comm (might be wrapped)
    String   commPath = "/proc/" + to_string(cred.pid) + "/comm";
    ifstream commFile(commPath);
    if (commFile >> compositorName) {
      subrange removedRange = std::ranges::remove(compositorName, '\n');
      compositorName.erase(removedRange.begin(), compositorName.end());
    }

    // 2. Check cmdline for actual binary reference
    String cmdline = ReadProcessCmdline(cred.pid);
    if (cmdline.contains("hyprland")) {
      wl_display_disconnect(display);
      return "Hyprland";
    }

    // 3. Check exe symlink
    String                exePath = "/proc/" + to_string(cred.pid) + "/exe";
    array<char, PATH_MAX> buf;
    ssize_t               lenBuf = readlink(exePath.c_str(), buf.data(), buf.size() - 1);
    if (lenBuf != -1) {
      buf.at(static_cast<usize>(lenBuf)) = '\0';
      String exe(buf.data());
      if (exe.contains("hyprland")) {
        wl_display_disconnect(display);
        return "Hyprland";
      }
    }

    wl_display_disconnect(display);

    // Final cleanup of wrapper names
    return TrimHyprlandWrapper(compositorName);
  }

  fn DetectFromEnvVars() -> optional<String> {
    // Use RAII to guard against concurrent env modifications
    static mutex      EnvMutex;
    lock_guard<mutex> lock(EnvMutex);

    // XDG_CURRENT_DESKTOP
    if (const char* xdgCurrentDesktop = getenv("XDG_CURRENT_DESKTOP")) {
      const string_view sview(xdgCurrentDesktop);
      const size_t      colon = sview.find(':');
      return String(sview.substr(0, colon)); // Direct construct from view
    }

    // DESKTOP_SESSION
    if (const char* desktopSession = getenv("DESKTOP_SESSION"))
      return String(string_view(desktopSession)); // Avoid intermediate view storage

    return nullopt;
  }

  fn DetectFromSessionFiles() -> optional<String> {
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
    String lowercaseStem;
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
          return String(patternIter->second);
      }
    }

    return nullopt;
  }

  fn DetectFromProcesses() -> optional<String> {
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
    String   envVars((istreambuf_iterator<char>(cmdline)), istreambuf_iterator<char>());

    for (const auto& [process, deName] : processChecks)
      if (envVars.contains(process))
        return String(deName);

    return nullopt;
  }

  fn GetMprisPlayers(const std::shared_ptr<DBus::Connection>& connection) -> expected<vector<String>, NowPlayingError> {
    try {
      // Create the method call object
      std::shared_ptr<DBus::CallMessage> call =
        DBus::CallMessage::create("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

      // Send the message synchronously and get the reply
      // Timeout parameter might be needed (e.g., 5000 ms)
      std::shared_ptr<DBus::Message> reply = connection->send_with_reply_blocking(call, 5000);

      // Check if the reply itself is an error type
      if (reply) {
        ERROR_LOG("DBus timeout or null reply in ListNames");
        return unexpected(LinuxError("DBus timeout in ListNames"));
      }

      vector<String>        allNamesStd;
      DBus::MessageIterator reader(*reply);
      reader >> allNamesStd;

      // Filter for MPRIS players
      vector<String> mprisPlayers; // Use std::string as String=std::string
      for (const auto& name : allNamesStd) {
        if (string_view(name).contains("org.mpris.MediaPlayer2")) {
          mprisPlayers.emplace_back(name);
        }
      }
      return mprisPlayers;
    } catch (const DBus::Error& e) { // Catch specific dbus-cxx exceptions
      ERROR_LOG("DBus::Error exception in ListNames: {}", e.what());
      return unexpected(LinuxError(e.what()));
    } catch (const std::exception& e) { // Catch other potential standard exceptions
      ERROR_LOG("Standard exception getting MPRIS players: {}", e.what());
      return unexpected(String(e.what()));
    }
  }

  // --- Logic remains the same ---
  fn GetActivePlayer(const vector<String>& mprisPlayers) -> optional<String> {
    if (!mprisPlayers.empty())
      return mprisPlayers.front();
    return nullopt;
  }
}

fn GetOSVersion() -> expected<String, String> {
  constexpr const char* path = "/etc/os-release";

  ifstream file(path);

  if (!file.is_open())
    return unexpected("Failed to open " + String(path));

  String       line;
  const String prefix = "PRETTY_NAME=";

  while (getline(file, line))
    if (line.starts_with(prefix)) {
      String prettyName = line.substr(prefix.size());

      if (!prettyName.empty() && prettyName.front() == '"' && prettyName.back() == '"')
        return prettyName.substr(1, prettyName.size() - 2);

      return prettyName;
    }

  return unexpected("PRETTY_NAME line not found in " + String(path));
}

fn GetMemInfo() -> expected<u64, String> {
  using std::from_chars, std::errc;

  constexpr const char* path = "/proc/meminfo";

  ifstream input(path);

  if (!input.is_open())
    return unexpected("Failed to open " + String(path));

  String line;
  while (getline(input, line)) {
    if (line.starts_with("MemTotal")) {
      const size_t colonPos = line.find(':');

      if (colonPos == String::npos)
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

  return unexpected("MemTotal line not found in " + String(path));
}

fn GetNowPlaying() -> expected<String, NowPlayingError> {
  try {
    // 1. Get Dispatcher and Session Bus Connection
    std::shared_ptr<DBus::Dispatcher> dispatcher = DBus::StandaloneDispatcher::create();
    if (!dispatcher)
      return unexpected(LinuxError("Failed to create DBus dispatcher"));

    std::shared_ptr<DBus::Connection> connection = dispatcher->create_connection(DBus::BusType::SESSION);
    if (!connection)
      return unexpected(LinuxError("Failed to connect to session bus"));

    // 2. Get list of MPRIS players
    auto mprisPlayersResult = GetMprisPlayers(connection);
    if (!mprisPlayersResult)
      return unexpected(mprisPlayersResult.error()); // Forward the error

    const vector<String>& mprisPlayers = *mprisPlayersResult;

    if (mprisPlayers.empty())
      return unexpected(NowPlayingError { NowPlayingCode::NoPlayers });

    // 3. Determine active player
    optional<String> activePlayerOpt = GetActivePlayer(mprisPlayers);
    if (!activePlayerOpt)
      return unexpected(NowPlayingError { NowPlayingCode::NoActivePlayer });

    // Use std::string for D-Bus service name
    const String& activePlayerService = *activePlayerOpt;

    // 4. Call Properties.Get for Metadata
    const String interfaceNameStd = "org.mpris.MediaPlayer2.Player";
    const String propertyNameStd  = "Metadata";

    // Create call message
    auto call = DBus::CallMessage::create(
      activePlayerService,               // Target service
      "/org/mpris/MediaPlayer2",         // Object path
      "org.freedesktop.DBus.Properties", // Interface
      "Get"
    ); // Method name

    (*call) << interfaceNameStd << propertyNameStd;

    // Send message and get reply
    std::shared_ptr<DBus::Message> replyMsg = connection->send_with_reply_blocking(call, 5000); // Use a timeout

    if (!replyMsg) {
      ERROR_LOG("DBus timeout or null reply in Properties.Get");
      return unexpected(LinuxError("DBus timeout in Properties.Get"));
    }

    // 5. Parse the reply
    DBus::Variant metadataVariant;
    // Create reader/iterator from the message
    DBus::MessageIterator reader(*replyMsg); // Use constructor
    // *** Correction: Use get<T> on iterator instead of operator>> ***
    reader >> metadataVariant;

    // Check the variant's signature
    if (metadataVariant.to_signature() != "a{sv}") {
      return unexpected("Unexpected reply type for Metadata");
    }
    String artistStd;
    String titleStd;

    // Get the dictionary using the templated get<T>() method
    std::map<String, DBus::Variant> metadataMap;
    auto                            titleIter = metadataMap.find("xesam:title");
    if (titleIter != metadataMap.end() && titleIter->second.to_signature() == "s") {
      // Use the cast operator on variant to string
      titleStd = static_cast<std::string>(titleIter->second);
    }

    // For line 525-534
    auto artistIter = metadataMap.find("xesam:artist");
    if (artistIter != metadataMap.end() && artistIter->second.to_signature() == "as") {
      // Cast to vector<String>
      std::vector<String> artistsStd = static_cast<std::vector<String>>(artistIter->second);
      if (!artistsStd.empty()) {
        artistStd = artistsStd.front();
      }
    }

    // 6. Construct result string
    String result;
    if (!artistStd.empty() && !titleStd.empty())
      result = artistStd + " - " + titleStd;
    else if (!titleStd.empty())
      result = titleStd;
    else if (!artistStd.empty())
      result = artistStd;

    return result;
  } catch (const DBus::Error& e) { // Catch specific dbus-cxx exceptions
    ERROR_LOG("DBus::Error exception in GetNowPlaying: {}", e.what());
    return unexpected(LinuxError(e.what()));
  } catch (const std::exception& e) { // Catch other potential standard exceptions
    ERROR_LOG("Standard exception in GetNowPlaying: {}", e.what());
    return unexpected(String(e.what()));
  }
}

fn GetWindowManager() -> String {
  // Check environment variables first
  const char* xdgSessionType = getenv("XDG_SESSION_TYPE");
  const char* waylandDisplay = getenv("WAYLAND_DISPLAY");

  // Prefer Wayland detection if Wayland session
  if ((waylandDisplay != nullptr) || (xdgSessionType && string_view(xdgSessionType).contains("wayland"))) {
    String compositor = GetWaylandCompositor();
    if (!compositor.empty())
      return compositor;

    // Fallback environment check
    const char* xdgCurrentDesktop = getenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop) {
      String desktop(xdgCurrentDesktop);
      transform(compositor, compositor.begin(), ::tolower);
      if (desktop.contains("hyprland"))
        return "hyprland";
    }
  }

  // X11 detection
  String x11wm = GetX11WindowManager();
  if (!x11wm.empty())
    return x11wm;

  return "Unknown";
}

fn GetDesktopEnvironment() -> optional<String> {
  // Try environment variables first
  if (auto desktopEnvironment = DetectFromEnvVars(); desktopEnvironment.has_value())
    return desktopEnvironment;

  // Try session files next
  if (auto desktopEnvironment = DetectFromSessionFiles(); desktopEnvironment.has_value())
    return desktopEnvironment;

  // Fallback to process detection
  return DetectFromProcesses();
}

fn GetShell() -> String {
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

  return !shell.empty() ? String(shell) : "";
}

fn GetHost() -> String {
  constexpr const char* path = "/sys/class/dmi/id/product_family";

  ifstream file(path);
  if (!file.is_open()) {
    ERROR_LOG("Failed to open {}", path);
    return "";
  }

  String productFamily;
  if (!getline(file, productFamily)) {
    ERROR_LOG("Failed to read from {}", path);
    return "";
  }

  return productFamily;
}

fn GetKernelVersion() -> String {
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
