#ifdef __WIN32__

#include <exception>
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>
#include <winrt/impl/Windows.Media.Control.2.h>

#include "os.h"

fn GetMemInfo() -> u64 {
  u64 mem = 0;
  GetPhysicallyInstalledSystemMemory(&mem);
  return mem * 1024;
}

fn GetNowPlaying() -> string {
  using namespace winrt::Windows::Media::Control;
  using namespace winrt::Windows::Foundation;

  using MediaProperties = GlobalSystemMediaTransportControlsSessionMediaProperties;
  using Session         = GlobalSystemMediaTransportControlsSession;
  using SessionManager  = GlobalSystemMediaTransportControlsSessionManager;

  try {
    // Request the session manager asynchronously
    const IAsyncOperation<SessionManager> sessionManagerOp = SessionManager::RequestAsync();
    const SessionManager                  sessionManager   = sessionManagerOp.get();

    if (const Session currentSession = sessionManager.GetCurrentSession()) {
      // Try to get the media properties asynchronously
      const MediaProperties mediaProperties = currentSession.TryGetMediaPropertiesAsync().get();

      // Convert the hstring title to string
      return to_string(mediaProperties.Title());
    }

    // If we reach this point, there is no current session
    return "No current media session.";
  } catch (...) { return "Failed to get media properties."; }
}

fn GetRegistryValue(const HKEY& hKey, const string& subKey, const string& valueName) -> string {
  HKEY key = nullptr;
  if (RegOpenKeyExA(hKey, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
    return "";

  DWORD dataSize = 0;
  if (RegQueryValueExA(key, valueName.c_str(), nullptr, nullptr, nullptr, &dataSize) !=
      ERROR_SUCCESS) {
    RegCloseKey(key);
    return "";
  }

  string value(dataSize, '\0');
  if (RegQueryValueExA(
        key,
        valueName.c_str(),
        nullptr,
        nullptr,
        reinterpret_cast<LPBYTE>(value.data()), // NOLINT(*-reinterpret-cast)
        &dataSize
      ) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return "";
  }

  RegCloseKey(key);
  // Remove null terminator if present
  if (!value.empty() && value.back() == '\0')
    value.pop_back();

  return value;
}

fn GetOSVersion() -> string {
  string productName = GetRegistryValue(
    HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "ProductName"
  );

  const string displayVersion = GetRegistryValue(
    HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "DisplayVersion"
  );

  const string releaseId = GetRegistryValue(
    HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "ReleaseId"
  );

  // Check for Windows 11
  if (const i32 buildNumber = stoi(GetRegistryValue(
        HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber"
      ));
      buildNumber >= 22000 && productName.find("Windows 10") != string::npos)
    productName.replace(productName.find("Windows 10"), 10, "Windows 11");

  if (!productName.empty()) {
    string result = productName;

    if (!displayVersion.empty())
      result += " " + displayVersion;
    else if (!releaseId.empty())
      result += " " + releaseId;

    return result;
  }

  return "";
}

#endif
