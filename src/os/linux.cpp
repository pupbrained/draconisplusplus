#ifdef __linux__

#include <SQLiteCpp/SQLiteCpp.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <expected>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <mutex>
#include <optional>
#include <ranges>
#include <sdbus-c++/Error.h>
#include <sdbus-c++/sdbus-c++.h>
#include <sqlite3.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <vector>
#include <wayland-client.h>

#include "os.h"
#include "src/util/macros.h"

using std::errc, std::exception, std::expected, std::from_chars, std::getline, std::istreambuf_iterator, std::less,
  std::lock_guard, std::map, std::mutex, std::ofstream, std::pair, std::string_view, std::vector, std::nullopt,
  std::array, std::unique_ptr, std::optional, std::bit_cast, std::to_string, std::ifstream, std::getenv, std::string,
  std::unexpected, std::ranges::is_sorted, std::ranges::lower_bound, std::ranges::replace, std::ranges::subrange,
  std::ranges::transform;

using namespace std::literals::string_view_literals;

namespace fs = std::filesystem;

enum SessionType : u8 { Wayland, X11, TTY, Unknown };

namespace {
  fn GetMprisPlayers(sdbus::IConnection& connection) -> vector<string> {
    const sdbus::ServiceName dbusInterface       = sdbus::ServiceName("org.freedesktop.DBus");
    const sdbus::ObjectPath  dbusObjectPath      = sdbus::ObjectPath("/org/freedesktop/DBus");
    const char*              dbusMethodListNames = "ListNames";

    const unique_ptr<sdbus::IProxy> dbusProxy = createProxy(connection, dbusInterface, dbusObjectPath);

    vector<string> names;

    dbusProxy->callMethod(dbusMethodListNames).onInterface(dbusInterface).storeResultsTo(names);

    vector<string> mprisPlayers;

    for (const string& name : names)
      if (const char* mprisInterfaceName = "org.mpris.MediaPlayer2"; name.find(mprisInterfaceName) != string::npos)
        mprisPlayers.push_back(name);

    return mprisPlayers;
  }

  fn GetActivePlayer(const vector<string>& mprisPlayers) -> optional<string> {
    if (!mprisPlayers.empty())
      return mprisPlayers.front();

    return nullopt;
  }

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
      memcpy(&wmWindow, data, sizeof(Window));
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
    if (input.find("hyprland") != string::npos)
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
    if (cmdline.find("hyprland") != string::npos) {
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
      if (exe.find("hyprland") != string::npos) {
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
      if (envVars.find(process) != string::npos)
        return string(deName);

    return nullopt;
  }

  fn CountNix() noexcept -> optional<usize> {
    constexpr string_view dbPath   = "/nix/var/nix/db/db.sqlite";
    constexpr string_view querySql = "SELECT COUNT(*) FROM ValidPaths WHERE sigs IS NOT NULL;";

    sqlite3*      sqlDB = nullptr;
    sqlite3_stmt* stmt  = nullptr;
    usize         count = 0;

    // 1. Direct URI construction without string concatenation
    const string uri = fmt::format("file:{}{}immutable=1", dbPath, (dbPath.find('?') != string_view::npos) ? "&" : "?");

    // 2. Open database with optimized flags
    if (sqlite3_open_v2(uri.c_str(), &sqlDB, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI | SQLITE_OPEN_NOMUTEX, nullptr) !=
        SQLITE_OK)
      return nullopt;

    // 3. Configure database for maximum read performance
    sqlite3_exec(sqlDB, "PRAGMA journal_mode=OFF; PRAGMA mmap_size=268435456;", nullptr, nullptr, nullptr);

    // 4. Single-step prepared statement execution
    if (sqlite3_prepare_v3(sqlDB, querySql.data(), querySql.size(), SQLITE_PREPARE_PERSISTENT, &stmt, nullptr) ==
        SQLITE_OK) {
      if (sqlite3_step(stmt) == SQLITE_ROW)
        count = static_cast<usize>(sqlite3_column_int64(stmt, 0));

      sqlite3_finalize(stmt);
    }

    sqlite3_close(sqlDB);
    return count ? optional { count } : nullopt;
  }

  fn CountNixWithCache() noexcept -> optional<size_t> {
    constexpr const char* dbPath    = "/nix/var/nix/db/db.sqlite";
    constexpr const char* cachePath = "/tmp/nix_pkg_count.cache";

    try {
      using mtime = fs::file_time_type;

      const mtime dbMtime    = fs::last_write_time(dbPath);
      const mtime cacheMtime = fs::last_write_time(cachePath);

      if (fs::exists(cachePath) && dbMtime <= cacheMtime) {
        ifstream cache(cachePath, std::ios::binary);
        size_t   count = 0;
        cache.read(bit_cast<char*>(&count), sizeof(count));
        return cache ? optional(count) : nullopt;
      }
    } catch (const exception& e) { DEBUG_LOG("Cache access failed: {}, rebuilding...", e.what()); }

    const optional<usize> count = CountNix();

    if (count) {
      constexpr const char* tmpPath = "/tmp/nix_pkg_count.tmp";

      {
        ofstream tmp(tmpPath, std::ios::binary | std::ios::trunc);
        tmp.write(bit_cast<const char*>(&*count), sizeof(*count));
      }

      fs::rename(tmpPath, cachePath);
    }

    return count;
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

fn GetNowPlaying() -> expected<string, NowPlayingError> {
  try {
    const char *playerObjectPath = "/org/mpris/MediaPlayer2", *playerInterfaceName = "org.mpris.MediaPlayer2.Player";

    unique_ptr<sdbus::IConnection> connection = sdbus::createSessionBusConnection();

    vector<string> mprisPlayers = GetMprisPlayers(*connection);

    if (mprisPlayers.empty())
      return unexpected(NowPlayingError { NowPlayingCode::NoPlayers });

    optional<string> activePlayer = GetActivePlayer(mprisPlayers);

    if (!activePlayer.has_value())
      return unexpected(NowPlayingError { NowPlayingCode::NoActivePlayer });

    unique_ptr<sdbus::IProxy> playerProxy =
      sdbus::createProxy(*connection, sdbus::ServiceName(*activePlayer), sdbus::ObjectPath(playerObjectPath));

    sdbus::Variant metadataVariant = playerProxy->getProperty("Metadata").onInterface(playerInterfaceName);

    if (metadataVariant.containsValueOfType<map<string, sdbus::Variant>>()) {
      const map<string, sdbus::Variant>& metadata = metadataVariant.get<map<string, sdbus::Variant>>();

      string title;
      auto   titleIter = metadata.find("xesam:title");
      if (titleIter != metadata.end() && titleIter->second.containsValueOfType<string>())
        title = titleIter->second.get<string>();

      string artist;
      auto   artistIter = metadata.find("xesam:artist");
      if (artistIter != metadata.end() && artistIter->second.containsValueOfType<vector<string>>()) {
        auto artists = artistIter->second.get<vector<string>>();
        if (!artists.empty())
          artist = artists[0];
      }

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
  } catch (const sdbus::Error& e) {
    if (e.getName() != "com.github.altdesktop.playerctld.NoActivePlayer")
      return unexpected(NowPlayingError { LinuxError(e) });

    return unexpected(NowPlayingError { NowPlayingCode::NoActivePlayer });
  }

  return "";
}

fn GetWindowManager() -> string {
  // Check environment variables first
  const char* xdgSessionType = getenv("XDG_SESSION_TYPE");
  const char* waylandDisplay = getenv("WAYLAND_DISPLAY");

  // Prefer Wayland detection if Wayland session
  if ((waylandDisplay != nullptr) || (xdgSessionType && strstr(xdgSessionType, "wayland"))) {
    string compositor = GetWaylandCompositor();
    if (!compositor.empty())
      return compositor;

    // Fallback environment check
    const char* xdgCurrentDesktop = getenv("XDG_CURRENT_DESKTOP");
    if (xdgCurrentDesktop) {
      string desktop(xdgCurrentDesktop);
      transform(compositor, compositor.begin(), ::tolower);
      if (desktop.find("hyprland") != string::npos)
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
  const char* shell = getenv("SHELL");

  return shell ? shell : "";
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

  DEBUG_LOG("{}", CountNixWithCache().value_or(0));

  return static_cast<const char*>(uts.release);
}

#endif
