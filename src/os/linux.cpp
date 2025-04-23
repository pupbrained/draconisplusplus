#ifdef __linux__

// clang-format off
#include <dbus-cxx.h> // needs to be at top for Success/None
// clang-format on
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <fstream>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <system_error>
#include <wayland-client.h>

#include "os.h"
#include "src/os/linux/display_guards.h"
#include "src/util/macros.h"

namespace fs = std::filesystem;

namespace {
  using os::linux::DisplayGuard;
  using os::linux::WaylandDisplayGuard;

  fn GetX11WindowManager() -> String {
    DisplayGuard display;

    if (!display)
      return "";

    Atom supportingWmCheck = XInternAtom(display.get(), "_NET_SUPPORTING_WM_CHECK", False);
    Atom wmName            = XInternAtom(display.get(), "_NET_WM_NAME", False);
    Atom utf8String        = XInternAtom(display.get(), "UTF8_STRING", False);

    Window root = display.defaultRootWindow();

    Window wmWindow     = 0;
    Atom   actualType   = 0;
    i32    actualFormat = 0;
    u64    nitems = 0, bytesAfter = 0;
    u8*    data = nullptr;

    if (XGetWindowProperty(
          display.get(),
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
      UniquePointer<u8, decltype(&XFree)> dataGuard(data, XFree);
      wmWindow = *std::bit_cast<Window*>(data);

      u8* nameData = nullptr;

      if (XGetWindowProperty(
            display.get(),
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
            &nameData
          ) == Success &&
          nameData) {
        UniquePointer<u8, decltype(&XFree)> nameGuard(nameData, XFree);
        return std::bit_cast<char*>(nameData);
      }
    }

    return "Unknown (X11)";
  }

  fn ReadProcessCmdline(i32 pid) -> String {
    std::ifstream cmdlineFile("/proc/" + std::to_string(pid) + "/cmdline");
    String        cmdline;

    if (getline(cmdlineFile, cmdline)) {
      std::ranges::replace(cmdline, '\0', ' ');
      return cmdline;
    }

    return "";
  }

  fn DetectHyprlandSpecific() -> String {
    Result<String, EnvError> xdgCurrentDesktop = GetEnv("XDG_CURRENT_DESKTOP");

    if (xdgCurrentDesktop) {
      std::ranges::transform(*xdgCurrentDesktop, xdgCurrentDesktop->begin(), ::tolower);

      if (xdgCurrentDesktop->contains("hyprland"))
        return "Hyprland";
    }

    if (GetEnv("HYPRLAND_INSTANCE_SIGNATURE"))
      return "Hyprland";

    if (fs::exists("/run/user/" + std::to_string(getuid()) + "/hypr"))
      return "Hyprland";

    return "";
  }

  fn GetWaylandCompositor() -> String {
    String hypr = DetectHyprlandSpecific();

    if (!hypr.empty())
      return hypr;

    WaylandDisplayGuard display;

    if (!display)
      return "";

    i32 fileDescriptor = display.fd();

    struct ucred cred;
    socklen_t    len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      return "";

    String compositorName;

    String        commPath = "/proc/" + std::to_string(cred.pid) + "/comm";
    std::ifstream commFile(commPath);
    if (commFile >> compositorName) {
      std::ranges::subrange removedRange = std::ranges::remove(compositorName, '\n');
      compositorName.erase(removedRange.begin(), removedRange.end());
    }

    String cmdline = ReadProcessCmdline(cred.pid);
    if (cmdline.contains("hyprland"))
      return "Hyprland";

    String                exePath = "/proc/" + std::to_string(cred.pid) + "/exe";
    Array<char, PATH_MAX> buf;
    ssize_t               lenBuf = readlink(exePath.c_str(), buf.data(), buf.size() - 1);
    if (lenBuf != -1) {
      buf.at(static_cast<usize>(lenBuf)) = '\0';
      String exe(buf.data());
      if (exe.contains("hyprland"))
        return "Hyprland";
    }

    return compositorName.contains("hyprland") ? "Hyprland" : compositorName;
  }

  fn DetectFromEnvVars() -> Option<String> {
    if (Result<String, EnvError> xdgCurrentDesktop = GetEnv("XDG_CURRENT_DESKTOP")) {
      const size_t colon = xdgCurrentDesktop->find(':');

      if (colon != String::npos)
        return xdgCurrentDesktop->substr(0, colon);

      return *xdgCurrentDesktop;
    }

    if (Result<String, EnvError> desktopSession = GetEnv("DESKTOP_SESSION"))
      return *desktopSession;

    return None;
  }

  fn DetectFromSessionFiles() -> Option<String> {
    // clang-format off
    static constexpr Array<Pair<StringView, StringView>, 12> DE_PATTERNS = {{
      {        "budgie",   "Budgie" },
      {      "cinnamon", "Cinnamon" },
      {          "lxqt",     "LXQt" },
      {          "mate",     "MATE" },
      {         "unity",    "Unity" },
      {         "gnome",    "GNOME" },
      { "gnome-wayland",    "GNOME" },
      {    "gnome-xorg",    "GNOME" },
      {           "kde",      "KDE" },
      {        "plasma",      "KDE" },
      {     "plasmax11",      "KDE" },
      {          "xfce",     "XFCE" },
    }};
    // clang-format on

    static constexpr Array<StringView, 2> SESSION_PATHS = { "/usr/share/xsessions", "/usr/share/wayland-sessions" };

    for (const StringView& path : SESSION_PATHS) {
      if (!fs::exists(path))
        continue;

      for (const fs::directory_entry& entry : fs::directory_iterator(path)) {
        if (!entry.is_regular_file())
          continue;

        String lowercaseStem = entry.path().stem().string();
        std::ranges::transform(lowercaseStem, lowercaseStem.begin(), ::tolower);

        for (const Pair pattern : DE_PATTERNS)
          if (pattern.first == lowercaseStem)
            return String(pattern.second);
      }
    }

    return None;
  }

  fn DetectFromProcesses() -> Option<String> {
    // clang-format off
    const Array<Pair<StringView, StringView>, 7> processChecks = {{
      {      "plasmashell",       "KDE" },
      {      "gnome-shell",     "GNOME" },
      {    "xfce4-session",      "XFCE" },
      {     "mate-session",      "MATE" },
      { "cinnamon-session",  "Cinnamon" },
      {        "budgie-wm",    "Budgie" },
      {     "lxqt-session",      "LXQt" },
    }};
    // clang-format on

    std::ifstream cmdline("/proc/self/environ");
    String        envVars((std::istreambuf_iterator<char>(cmdline)), std::istreambuf_iterator<char>());

    for (const auto& [process, deName] : processChecks)
      if (envVars.contains(process))
        return String(deName);

    return None;
  }

  fn GetMprisPlayers(const SharedPointer<DBus::Connection>& connection) -> Result<Vec<String>, NowPlayingError> {
    try {
      SharedPointer<DBus::CallMessage> call =
        DBus::CallMessage::create("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");

      SharedPointer<DBus::Message> reply = connection->send_with_reply_blocking(call, 500);

      if (!reply) {
        ERROR_LOG("DBus timeout or null reply in ListNames");
        return Err("DBus timeout in ListNames");
      }

      Vec<String>           allNamesStd;
      DBus::MessageIterator reader(*reply);
      reader >> allNamesStd;

      Vec<String> mprisPlayers;
      for (const String& name : allNamesStd)
        if (StringView(name).contains("org.mpris.MediaPlayer2"))
          mprisPlayers.emplace_back(name);

      return mprisPlayers;
    } catch (const DBus::Error& e) {
      ERROR_LOG("DBus::Error exception in ListNames: {}", e.what());
      return Err(e.what());
    } catch (const Exception& e) {
      ERROR_LOG("Standard exception getting MPRIS players: {}", e.what());
      return Err(e.what());
    }
  }

}

fn GetOSVersion() -> Result<String, String> {
  constexpr CStr path = "/etc/os-release";

  std::ifstream file(path);

  if (!file)
    return Err(std::format("Failed to open {}", path));

  String       line;
  const String prefix = "PRETTY_NAME=";

  while (getline(file, line))
    if (line.starts_with(prefix)) {
      StringView valueView = StringView(line).substr(prefix.size());

      if (!valueView.empty() && valueView.front() == '"' && valueView.back() == '"') {
        valueView.remove_prefix(1);
        valueView.remove_suffix(1);
      }

      return String(valueView);
    }

  return Err(std::format("PRETTY_NAME line not found in {}", path));
}

fn GetMemInfo() -> Result<u64, String> {
  struct sysinfo info;

  if (sysinfo(&info) != 0)
    return Err(std::format("sysinfo failed: {}", std::error_code(errno, std::generic_category()).message()));

  return static_cast<u64>(info.totalram * info.mem_unit);
}

fn GetNowPlaying() -> Result<String, NowPlayingError> {
  try {
    SharedPointer<DBus::Dispatcher> dispatcher = DBus::StandaloneDispatcher::create();
    if (!dispatcher)
      return Err("Failed to create DBus dispatcher");

    SharedPointer<DBus::Connection> connection = dispatcher->create_connection(DBus::BusType::SESSION);
    if (!connection)
      return Err("Failed to connect to session bus");

    Result<Vec<String>, NowPlayingError> mprisPlayersResult = GetMprisPlayers(connection);
    if (!mprisPlayersResult)
      return Err(mprisPlayersResult.error());

    const Vec<String>& mprisPlayers = *mprisPlayersResult;

    if (mprisPlayers.empty())
      return Err(NowPlayingCode::NoPlayers);

    String activePlayer = mprisPlayers.front();

    SharedPointer<DBus::CallMessage> metadataCall =
      DBus::CallMessage::create(activePlayer, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", "Get");

    (*metadataCall) << "org.mpris.MediaPlayer2.Player" << "Metadata";

    SharedPointer<DBus::Message> metadataReply = connection->send_with_reply_blocking(metadataCall, 5000);

    String title;
    String artist;

    if (metadataReply && metadataReply->is_valid()) {
      try {
        DBus::MessageIterator iter(*metadataReply);
        DBus::Variant         metadataVariant;
        iter >> metadataVariant;

        if (metadataVariant.type() == DBus::DataType::ARRAY) {
          Map<String, DBus::Variant> metadata = metadataVariant.to_map<String, DBus::Variant>();

          auto titleIter = metadata.find("xesam:title");
          if (titleIter != metadata.end() && titleIter->second.type() == DBus::DataType::STRING)
            title = titleIter->second.to_string();

          auto artistIter = metadata.find("xesam:artist");
          if (artistIter != metadata.end()) {
            if (artistIter->second.type() == DBus::DataType::ARRAY) {
              if (Vec<String> artists = artistIter->second.to_vector<String>(); !artists.empty())
                artist = artists[0];
            } else if (artistIter->second.type() == DBus::DataType::STRING)
              artist = artistIter->second.to_string();
          }
        } else {
          ERROR_LOG(
            "Metadata variant is not the expected type, expected a{{sv}} but got {}", metadataVariant.signature().str()
          );
        }
      } catch (const DBus::Error& e) {
        ERROR_LOG("DBus error processing metadata reply: {}", e.what());
      } catch (const Exception& e) { ERROR_LOG("Error processing metadata reply: {}", e.what()); }
    }

    return std::format("{}{}{}", artist, (!artist.empty() && !title.empty()) ? " - " : "", title);
  } catch (const DBus::Error& e) { return Err(std::format("DBus error: {}", e.what())); } catch (const Exception& e) {
    return Err(std::format("General error: {}", e.what()));
  }
}

fn GetWindowManager() -> String {
  const Result<String, EnvError> waylandDisplay = GetEnv("WAYLAND_DISPLAY");
  const Result<String, EnvError> xdgSessionType = GetEnv("XDG_SESSION_TYPE");

  if (waylandDisplay || (xdgSessionType && xdgSessionType->contains("wayland"))) {
    String compositor = GetWaylandCompositor();
    if (!compositor.empty())
      return compositor;

    if (const Result<String, EnvError> xdgCurrentDesktop = GetEnv("XDG_CURRENT_DESKTOP")) {
      std::ranges::transform(compositor, compositor.begin(), ::tolower);
      if (xdgCurrentDesktop->contains("hyprland"))
        return "Hyprland";
    }
  }

  if (String x11wm = GetX11WindowManager(); !x11wm.empty())
    return x11wm;

  return "Unknown";
}

fn GetDesktopEnvironment() -> Option<String> {
  if (Option<String> desktopEnvironment = DetectFromEnvVars())
    return desktopEnvironment;

  if (Option<String> desktopEnvironment = DetectFromSessionFiles())
    return desktopEnvironment;

  return DetectFromProcesses();
}

fn GetShell() -> String {
  const Vec<Pair<String, String>> shellMap {
    { "bash",    "Bash" },
    {  "zsh",     "Zsh" },
    { "fish",    "Fish" },
    {   "nu", "Nushell" },
    {   "sh",      "SH" }, // sh last because other shells contain "sh"
  };

  if (const Result<String, EnvError> shellPath = GetEnv("SHELL")) {
    for (const auto& shellPair : shellMap)
      if (shellPath->contains(shellPair.first))
        return shellPair.second;

    return *shellPath; // fallback to the raw shell path
  }

  return "";
}

fn GetHost() -> String {
  constexpr CStr path = "/sys/class/dmi/id/product_family";

  std::ifstream file(path);

  if (!file) {
    ERROR_LOG("Failed to open {}", path);
    return "";
  }

  String productFamily;

  if (!getline(file, productFamily)) {
    ERROR_LOG("Failed to read from {} (is it empty?)", path);
    return "";
  }

  return productFamily.erase(productFamily.find_last_not_of(" \t\n\r") + 1);
}

fn GetKernelVersion() -> String {
  struct utsname uts;

  if (uname(&uts) == -1) {
    ERROR_LOG("uname() failed: {}", std::error_code(errno, std::generic_category()).message());
    return "";
  }

  return static_cast<CStr>(uts.release);
}

fn GetDiskUsage() -> Pair<u64, u64> {
  struct statvfs stat;

  if (statvfs("/", &stat) == -1) {
    ERROR_LOG("statvfs() failed: {}", std::error_code(errno, std::generic_category()).message());
    return { 0, 0 };
  }

  return { (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize), stat.f_blocks * stat.f_frsize };
}

#endif
