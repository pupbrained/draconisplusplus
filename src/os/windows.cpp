#ifdef __WIN32__

#include <iostream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>
#include <winrt/impl/Windows.Media.Control.2.h>

// clang-format off
#include <dwmapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <vector>
#include <cstring>
// clang-format on

#include "os.h"

using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

namespace {
  fn GetRegistryValue(const HKEY& hKey, const string& subKey, const string& valueName) -> string {
    HKEY key = nullptr;
    if (RegOpenKeyExA(hKey, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
      return "";

    DWORD dataSize = 0;
    if (RegQueryValueExA(key, valueName.c_str(), nullptr, nullptr, nullptr, &dataSize) != ERROR_SUCCESS) {
      RegCloseKey(key);
      return "";
    }

    string value(dataSize, '\0');
    if (RegQueryValueExA(key, valueName.c_str(), nullptr, nullptr, std::bit_cast<LPBYTE>(value.data()), &dataSize) !=
        ERROR_SUCCESS) {
      RegCloseKey(key);
      return "";
    }

    RegCloseKey(key);

    // Remove null terminator if present
    if (!value.empty() && value.back() == '\0')
      value.pop_back();

    return value;
  }

  // Add these function implementations
  fn GetRunningProcesses() -> std::vector<string> {
    std::vector<string> processes;
    HANDLE              hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot == INVALID_HANDLE_VALUE)
      return processes;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe32)) {
      CloseHandle(hSnapshot);
      return processes;
    }

    while (Process32Next(hSnapshot, &pe32)) processes.emplace_back(pe32.szExeFile);

    CloseHandle(hSnapshot);
    return processes;
  }

  fn IsProcessRunning(const std::vector<string>& processes, const string& name) -> bool {
    return std::ranges::any_of(processes, [&name](const string& proc) {
      return _stricmp(proc.c_str(), name.c_str()) == 0;
    });
  }
}

fn GetMemInfo() -> expected<u64, string> {
  u64 mem = 0;

  if (!GetPhysicallyInstalledSystemMemory(&mem))
    return std::unexpected("Failed to get physical system memory.");

  return mem * 1024;
}

fn GetNowPlaying() -> expected<string, NowPlayingError> {
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
    return std::unexpected(NowPlayingError { NowPlayingCode::NoActivePlayer });
  } catch (const winrt::hresult_error& e) { return std::unexpected(NowPlayingError { e }); }
}

fn GetOSVersion() -> expected<string, string> {
  string productName =
    GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "ProductName");

  const string displayVersion =
    GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "DisplayVersion");

  const string releaseId =
    GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "ReleaseId");

  // Check for Windows 11
  if (const i32 buildNumber = stoi(
        GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber")
      );
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

  return std::unexpected("Failed to get OS version.");
}

fn GetHost() -> string {
  string hostName = GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SYSTEM\HardwareConfig\Current)", "SystemFamily");

  if (hostName.empty())
    hostName = GetRegistryValue(
      HKEY_LOCAL_MACHINE, R"(SYSTEM\CurrentControlSet\Control\ComputerName\ComputerName)", "ComputerName"
    );

  return hostName;
}

fn GetKernelVersion() -> string {
  std::stringstream versionStream;
  HMODULE           ntdllHandle = GetModuleHandleW(L"ntdll.dll");

  if (ntdllHandle) {
    auto rtlGetVersion = std::bit_cast<RtlGetVersionPtr>(GetProcAddress(ntdllHandle, "RtlGetVersion"));
    if (rtlGetVersion) {
      RTL_OSVERSIONINFOW osInfo = {};

      osInfo.dwOSVersionInfoSize = sizeof(osInfo);

      if (rtlGetVersion(&osInfo) == 0)
        versionStream << osInfo.dwMajorVersion << "." << osInfo.dwMinorVersion << "." << osInfo.dwBuildNumber << "."
                      << osInfo.dwPlatformId;
    }
  }

  return versionStream.str();
}

fn GetWindowManager() -> string {
  const std::vector<string> processes = GetRunningProcesses();
  string                    windowManager;

  // Check for third-party WMs
  if (IsProcessRunning(processes, "glazewm.exe")) {
    windowManager = "GlazeWM";
  } else if (IsProcessRunning(processes, "fancywm.exe")) {
    windowManager = "FancyWM";
  } else if (IsProcessRunning(processes, "komorebi.exe") || IsProcessRunning(processes, "komorebic.exe")) {
    windowManager = "Komorebi";
  }

  // Fallback to DWM detection
  if (windowManager.empty()) {
    BOOL compositionEnabled = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled))) {
      windowManager = compositionEnabled ? "Desktop Window Manager" : "Windows Manager (Basic)";
    } else {
      windowManager = "Windows Manager";
    }
  }

  return windowManager;
}

fn GetDesktopEnvironment() -> optional<string> {
  // Get version information from registry
  const string buildStr =
    GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber");

  if (buildStr.empty())
    return std::nullopt;

  try {
    const i32 build = stoi(buildStr);

    // Windows 11+ (Fluent)
    if (build >= 22000)
      return "Fluent (Windows 11)";

    // Windows 10 Fluent Era
    if (build >= 15063)
      return "Fluent (Windows 10)";

    // Windows 8.1/10 Metro Era
    if (build >= 9200) { // Windows 8+
      // Distinguish between Windows 8 and 10
      const string productName =
        GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "ProductName");

      if (productName.find("Windows 10") != string::npos)
        return "Metro (Windows 10)";

      if (build >= 9600)
        return "Metro (Windows 8.1)";

      return "Metro (Windows 8)";
    }

    // Windows 7 Aero
    if (build >= 7600)
      return "Aero (Windows 7)";

    // Older versions
    return "Classic";
  } catch (...) { return std::nullopt; }
}

#endif
