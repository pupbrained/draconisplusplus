#ifdef __linux__

// clang-format off
#include <cstring>                          // std::strlen
#include <dbus/dbus.h>
#include <expected>                         // std::{unexpected, expected}
#include <format>                           // std::{format, format_to_n}
#include <fstream>                          // std::ifstream
#include <climits>                          // PATH_MAX
#include <limits>                           // std::numeric_limits
#include <string>                           // std::{getline, string (String)}
#include <string_view>                      // std::string_view (StringView)
#include <sys/socket.h>                     // ucred, getsockopt, SOL_SOCKET, SO_PEERCRED
#include <sys/statvfs.h>                    // statvfs
#include <sys/sysinfo.h>                    // sysinfo
#include <sys/utsname.h>                    // utsname, uname
#include <unistd.h>                         // readlink

#include "src/core/util/helpers.hpp"
#include "src/core/util/logging.hpp"

#include "src/wrappers/wayland.hpp"
#include "src/wrappers/xcb.hpp"

#include "os.hpp"
#include "linux/pkg_count.hpp"
// clang-format on

using namespace util::types;
using util::error::DraconisError, util::error::DraconisErrorCode;

namespace {
  fn GetX11WindowManager() -> Result<String, DraconisError> {
    using namespace xcb;

    const DisplayGuard conn;

    if (!conn)
      if (const i32 err = connection_has_error(conn.get()))
        return Err(DraconisError(DraconisErrorCode::ApiUnavailable, [&] -> String {
          if (const Option<ConnError> connErr = getConnError(err)) {
            switch (*connErr) {
              case Generic:         return "Stream/Socket/Pipe Error";
              case ExtNotSupported: return "Extension Not Supported";
              case MemInsufficient: return "Insufficient Memory";
              case ReqLenExceed:    return "Request Length Exceeded";
              case ParseErr:        return "Display String Parse Error";
              case InvalidScreen:   return "Invalid Screen";
              case FdPassingFailed: return "FD Passing Failed";
              default:              return std::format("Unknown Error Code ({})", err);
            }
          }

          return std::format("Unknown Error Code ({})", err);
        }()));

    fn internAtom = [&conn](const StringView name) -> Result<atom_t, DraconisError> {
      const ReplyGuard<intern_atom_reply_t> reply(
        intern_atom_reply(conn.get(), intern_atom(conn.get(), 0, static_cast<u16>(name.size()), name.data()), nullptr)
      );

      if (!reply)
        return Err(
          DraconisError(DraconisErrorCode::PlatformSpecific, std::format("Failed to get X11 atom reply for '{}'", name))
        );

      return reply->atom;
    };

    const Result<atom_t, DraconisError> supportingWmCheckAtom = internAtom("_NET_SUPPORTING_WM_CHECK");
    const Result<atom_t, DraconisError> wmNameAtom            = internAtom("_NET_WM_NAME");
    const Result<atom_t, DraconisError> utf8StringAtom        = internAtom("UTF8_STRING");

    if (!supportingWmCheckAtom || !wmNameAtom || !utf8StringAtom) {
      if (!supportingWmCheckAtom)
        error_log("Failed to get _NET_SUPPORTING_WM_CHECK atom");

      if (!wmNameAtom)
        error_log("Failed to get _NET_WM_NAME atom");

      if (!utf8StringAtom)
        error_log("Failed to get UTF8_STRING atom");

      return Err(DraconisError(DraconisErrorCode::PlatformSpecific, "Failed to get X11 atoms"));
    }

    const ReplyGuard<get_property_reply_t> wmWindowReply(get_property_reply(
      conn.get(),
      get_property(conn.get(), 0, conn.rootScreen()->root, *supportingWmCheckAtom, ATOM_WINDOW, 0, 1),
      nullptr
    ));

    if (!wmWindowReply || wmWindowReply->type != ATOM_WINDOW || wmWindowReply->format != 32 ||
        get_property_value_length(wmWindowReply.get()) == 0)
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to get _NET_SUPPORTING_WM_CHECK property"));

    const window_t wmRootWindow = *static_cast<window_t*>(get_property_value(wmWindowReply.get()));

    const ReplyGuard<get_property_reply_t> wmNameReply(get_property_reply(
      conn.get(), get_property(conn.get(), 0, wmRootWindow, *wmNameAtom, *utf8StringAtom, 0, 1024), nullptr
    ));

    if (!wmNameReply || wmNameReply->type != *utf8StringAtom || get_property_value_length(wmNameReply.get()) == 0)
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to get _NET_WM_NAME property"));

    const char* nameData = static_cast<const char*>(get_property_value(wmNameReply.get()));
    const usize length   = get_property_value_length(wmNameReply.get());

    return String(nameData, length);
  }

  fn GetWaylandCompositor() -> Result<String, DraconisError> {
    const wl::DisplayGuard display;

    if (!display)
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to connect to display (is Wayland running?)"));

    const i32 fileDescriptor = display.fd();
    if (fileDescriptor < 0)
      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "Failed to get Wayland file descriptor"));

    ucred     cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(fileDescriptor, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1)
      return Err(DraconisError::withErrno("Failed to get socket credentials (SO_PEERCRED)"));

    Array<char, 128> exeLinkPathBuf;

    auto [out, size] = std::format_to_n(exeLinkPathBuf.data(), exeLinkPathBuf.size() - 1, "/proc/{}/exe", cred.pid);

    if (out >= exeLinkPathBuf.data() + exeLinkPathBuf.size() - 1)
      return Err(DraconisError(DraconisErrorCode::InternalError, "Failed to format /proc path (PID too large?)"));

    *out = '\0';

    const char* exeLinkPath = exeLinkPathBuf.data();

    Array<char, PATH_MAX> exeRealPathBuf;

    const isize bytesRead = readlink(exeLinkPath, exeRealPathBuf.data(), exeRealPathBuf.size() - 1);

    if (bytesRead == -1)
      return Err(DraconisError::withErrno(std::format("Failed to read link '{}'", exeLinkPath)));

    exeRealPathBuf.at(bytesRead) = '\0';

    StringView compositorNameView;

    const StringView pathView(exeRealPathBuf.data(), bytesRead);

    StringView filenameView;

    if (const usize lastCharPos = pathView.find_last_not_of('/'); lastCharPos != StringView::npos) {
      const StringView relevantPart = pathView.substr(0, lastCharPos + 1);

      if (const usize separatorPos = relevantPart.find_last_of('/'); separatorPos == StringView::npos)
        filenameView = relevantPart;
      else
        filenameView = relevantPart.substr(separatorPos + 1);
    }

    if (!filenameView.empty())
      compositorNameView = filenameView;

    if (compositorNameView.empty() || compositorNameView == "." || compositorNameView == "/")
      return Err(DraconisError(DraconisErrorCode::NotFound, "Failed to get compositor name from path"));

    if (constexpr StringView wrappedSuffix = "-wrapped"; compositorNameView.length() > 1 + wrappedSuffix.length() &&
        compositorNameView[0] == '.' && compositorNameView.ends_with(wrappedSuffix)) {
      const StringView cleanedView =
        compositorNameView.substr(1, compositorNameView.length() - 1 - wrappedSuffix.length());

      if (cleanedView.empty())
        return Err(DraconisError(DraconisErrorCode::NotFound, "Compositor name invalid after heuristic"));

      return String(cleanedView);
    }

    return String(compositorNameView);
  }

  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wold-style-cast"
  fn GetMprisPlayers(DBusConnection* connection) -> Vec<String> {
    Vec<String> mprisPlayers;
    DBusError   err;
    dbus_error_init(&err);

    // Create a method call to org.freedesktop.DBus.ListNames
    DBusMessage* msg = dbus_message_new_method_call(
      "org.freedesktop.DBus",  // target service
      "/org/freedesktop/DBus", // object path
      "org.freedesktop.DBus",  // interface name
      "ListNames"              // method name
    );

    if (!msg) {
      debug_log("Failed to create message for ListNames.");
      return mprisPlayers;
    }

    // Send the message and block until we get a reply.
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
      debug_log("DBus error in ListNames: {}", err.message);
      dbus_error_free(&err);
      return mprisPlayers;
    }

    if (!reply) {
      debug_log("No reply received for ListNames.");
      return mprisPlayers;
    }

    // The expected reply signature is "as" (an array of strings)
    DBusMessageIter iter;

    if (!dbus_message_iter_init(reply, &iter)) {
      debug_log("Reply has no arguments.");
      dbus_message_unref(reply);
      return mprisPlayers;
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_ARRAY) {
      debug_log("Reply argument is not an array.");
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

  fn GetActivePlayer(const Vec<String>& mprisPlayers) -> Option<String> {
    if (!mprisPlayers.empty())
      return mprisPlayers.front();
    return None;
  }
} // namespace

namespace os {
  fn GetOSVersion() -> Result<String, DraconisError> {
    constexpr CStr path = "/etc/os-release";

    std::ifstream file(path);

    if (!file)
      return Err(DraconisError(DraconisErrorCode::NotFound, std::format("Failed to open {}", path)));

    String               line;
    constexpr StringView prefix = "PRETTY_NAME=";

    while (std::getline(file, line)) {
      if (StringView(line).starts_with(prefix)) {
        String value = line.substr(prefix.size());

        if ((value.length() >= 2 && value.front() == '"' && value.back() == '"') ||
            (value.length() >= 2 && value.front() == '\'' && value.back() == '\''))
          value = value.substr(1, value.length() - 2);

        if (value.empty())
          return Err(DraconisError(
            DraconisErrorCode::ParseError, std::format("PRETTY_NAME value is empty or only quotes in {}", path)
          ));

        return value;
      }
    }

    return Err(DraconisError(DraconisErrorCode::NotFound, std::format("PRETTY_NAME line not found in {}", path)));
  }

  fn GetMemInfo() -> Result<u64, DraconisError> {
    struct sysinfo info;

    if (sysinfo(&info) != 0)
      return Err(DraconisError::withErrno("sysinfo call failed"));

    const u64 totalRam = info.totalram;
    const u64 memUnit  = info.mem_unit;

    if (memUnit == 0)
      return Err(DraconisError(DraconisErrorCode::InternalError, "sysinfo returned mem_unit of zero"));

    if (totalRam > std::numeric_limits<u64>::max() / memUnit)
      return Err(DraconisError(DraconisErrorCode::InternalError, "Potential overflow calculating total RAM"));

    return info.totalram * info.mem_unit;
  }

  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wold-style-cast"
  fn GetNowPlaying() -> Result<MediaInfo, DraconisError> {
    DBusError err;
    dbus_error_init(&err);

    // Connect to the session bus
    DBusConnection* connection = dbus_bus_get(DBUS_BUS_SESSION, &err);

    if (!connection)
      if (dbus_error_is_set(&err)) {
        error_log("DBus connection error: {}", err.message);

        DraconisError error = DraconisError(DraconisErrorCode::ApiUnavailable, err.message);
        dbus_error_free(&err);

        return Err(error);
      }

    Vec<String> mprisPlayers = GetMprisPlayers(connection);

    if (mprisPlayers.empty()) {
      dbus_connection_unref(connection);
      return Err(DraconisError(DraconisErrorCode::NotFound, "No MPRIS players found"));
    }

    Option<String> activePlayer = GetActivePlayer(mprisPlayers);

    if (!activePlayer.has_value()) {
      dbus_connection_unref(connection);
      return Err(DraconisError(DraconisErrorCode::NotFound, "No active MPRIS player found"));
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
      return Err(DraconisError(DraconisErrorCode::InternalError, "Failed to create DBus message"));
    }

    const char* interfaceName = "org.mpris.MediaPlayer2.Player";
    const char* propertyName  = "Metadata";

    if (!dbus_message_append_args(
          msg, DBUS_TYPE_STRING, &interfaceName, DBUS_TYPE_STRING, &propertyName, DBUS_TYPE_INVALID
        )) {
      dbus_message_unref(msg);
      dbus_connection_unref(connection);
      return Err(DraconisError(DraconisErrorCode::InternalError, "Failed to append arguments to DBus message"));
    }

    // Call the method and block until reply is received.
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(connection, msg, -1, &err);
    dbus_message_unref(msg);

    if (dbus_error_is_set(&err)) {
      error_log("DBus error in Properties.Get: {}", err.message);

      DraconisError error = DraconisError(DraconisErrorCode::ApiUnavailable, err.message);
      dbus_error_free(&err);
      dbus_connection_unref(connection);

      return Err(error);
    }

    if (!reply) {
      dbus_connection_unref(connection);
      return Err(DraconisError(DraconisErrorCode::ApiUnavailable, "No reply received for Properties.Get"));
    }

    // The reply should contain a variant holding a dictionary ("a{sv}")
    DBusMessageIter iter;

    if (!dbus_message_iter_init(reply, &iter)) {
      dbus_message_unref(reply);
      dbus_connection_unref(connection);
      return Err(DraconisError(DraconisErrorCode::InternalError, "Reply has no arguments"));
    }

    if (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT) {
      dbus_message_unref(reply);
      dbus_connection_unref(connection);
      return Err(DraconisError(DraconisErrorCode::InternalError, "Reply argument is not a variant"));
    }

    // Recurse into the variant to get the dictionary
    DBusMessageIter variantIter;
    dbus_message_iter_recurse(&iter, &variantIter);

    if (dbus_message_iter_get_arg_type(&variantIter) != DBUS_TYPE_ARRAY) {
      dbus_message_unref(reply);
      dbus_connection_unref(connection);
      return Err(DraconisError(DraconisErrorCode::InternalError, "Variant argument is not an array"));
    }

    String          title;
    String          artist;
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

    return MediaInfo(artist, title);
  }
  #pragma clang diagnostic pop

  fn GetWindowManager() -> Option<String> {
    if (Result<String, DraconisError> waylandResult = GetWaylandCompositor())
      return *waylandResult;
    else
      debug_log("Could not detect Wayland compositor: {}", waylandResult.error().message);

    if (Result<String, DraconisError> x11Result = GetX11WindowManager())
      return *x11Result;
    else
      debug_log("Could not detect X11 window manager: {}", x11Result.error().message);

    return None;
  }

  fn GetDesktopEnvironment() -> Option<String> {
    return util::helpers::GetEnv("XDG_CURRENT_DESKTOP")
      .transform([](const String& xdgDesktop) -> String {
        if (const usize colon = xdgDesktop.find(':'); colon != String::npos)
          return xdgDesktop.substr(0, colon);

        return xdgDesktop;
      })
      .or_else([](const DraconisError&) -> Result<String, DraconisError> {
        return util::helpers::GetEnv("DESKTOP_SESSION");
      })
      .transform([](const String& finalValue) -> Option<String> {
        debug_log("Found desktop environment: {}", finalValue);
        return finalValue;
      })
      .value_or(None);
  }

  fn GetShell() -> Option<String> {
    if (const Result<String, DraconisError> shellPath = util::helpers::GetEnv("SHELL")) {
      // clang-format off
    constexpr Array<Pair<StringView, StringView>, 5> shellMap {{
      { "bash",    "Bash" },
      {  "zsh",     "Zsh" },
      { "fish",    "Fish" },
      {   "nu", "Nushell" },
      {   "sh",      "SH" }, // sh last because other shells contain "sh"
    }};
      // clang-format on

      for (const auto& [exe, name] : shellMap)
        if (shellPath->contains(exe))
          return String(name);

      return *shellPath; // fallback to the raw shell path
    }

    return None;
  }

  fn GetHost() -> Result<String, DraconisError> {
    constexpr CStr primaryPath  = "/sys/class/dmi/id/product_family";
    constexpr CStr fallbackPath = "/sys/class/dmi/id/product_name";

    fn readFirstLine = [&](const String& path) -> Result<String, DraconisError> {
      std::ifstream file(path);
      String        line;

      if (!file)
        return Err(DraconisError(
          DraconisErrorCode::NotFound, std::format("Failed to open DMI product identifier file '{}'", path)
        ));

      if (!std::getline(file, line))
        return Err(
          DraconisError(DraconisErrorCode::ParseError, std::format("DMI product identifier file ('{}') is empty", path))
        );

      return line;
    };

    return readFirstLine(primaryPath).or_else([&](const DraconisError& primaryError) -> Result<String, DraconisError> {
      return readFirstLine(fallbackPath)
        .or_else([&](const DraconisError& fallbackError) -> Result<String, DraconisError> {
          return Err(DraconisError(
            DraconisErrorCode::InternalError,
            std::format(
              "Failed to get host identifier. Primary ('{}'): {}. Fallback ('{}'): {}",
              primaryPath,
              primaryError.message,
              fallbackPath,
              fallbackError.message
            )
          ));
        });
    });
  }

  fn GetKernelVersion() -> Result<String, DraconisError> {
    utsname uts;

    if (uname(&uts) == -1)
      return Err(DraconisError::withErrno("uname call failed"));

    if (std::strlen(uts.release) == 0)
      return Err(DraconisError(DraconisErrorCode::ParseError, "uname returned null kernel release"));

    return uts.release;
  }

  fn GetDiskUsage() -> Result<DiskSpace, DraconisError> {
    struct statvfs stat;

    if (statvfs("/", &stat) == -1)
      return Err(DraconisError::withErrno(std::format("Failed to get filesystem stats for '/' (statvfs call failed)")));

    return DiskSpace {
      .used_bytes  = (stat.f_blocks * stat.f_frsize) - (stat.f_bfree * stat.f_frsize),
      .total_bytes = stat.f_blocks * stat.f_frsize,
    };
  }

  fn GetPackageCount() -> Result<u64, DraconisError> { return linux::GetTotalPackageCount(); }
} // namespace os

#endif // __linux__
