#ifdef _WIN32

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <dwmapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <vector>
#include <cstring>
// clang-format on

#include <guiddef.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/base.h>
#include <winrt/impl/Windows.Media.Control.2.h>

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

  fn GetParentProcessId(DWORD pid) -> DWORD {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
      return 0;

    PROCESSENTRY32 pe32;
    pe32.dwSize     = sizeof(PROCESSENTRY32);
    DWORD parentPid = 0;

    if (Process32First(hSnapshot, &pe32)) {
      while (true) {
        if (pe32.th32ProcessID == pid) {
          parentPid = pe32.th32ParentProcessID;
          break;
        }
        if (!Process32Next(hSnapshot, &pe32)) {
          break;
        }
      }
    }
    CloseHandle(hSnapshot);
    return parentPid;
  }

  fn GetProcessName(DWORD pid) -> string {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
      return "";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    string processName;

    if (Process32First(hSnapshot, &pe32)) {
      while (true) {
        if (pe32.th32ProcessID == pid) {
          // Explicitly cast array to string to avoid implicit array decay
          processName = string(reinterpret_cast<const char*>(pe32.szExeFile));
          break;
        }

        if (!Process32Next(hSnapshot, &pe32))
          break;
      }
    }
    CloseHandle(hSnapshot);
    return processName;
  }
}

fn GetMemInfo() -> expected<u64, string> {
  MEMORYSTATUSEX memInfo;
  memInfo.dwLength = sizeof(MEMORYSTATUSEX);
  if (!GlobalMemoryStatusEx(&memInfo)) {
    return std::unexpected("Failed to get memory status");
  }
  return memInfo.ullTotalPhys;
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

  return "Windows";
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
  if (IsProcessRunning(processes, "glazewm.exe"))
    windowManager = "GlazeWM";
  else if (IsProcessRunning(processes, "fancywm.exe"))
    windowManager = "FancyWM";
  else if (IsProcessRunning(processes, "komorebi.exe") || IsProcessRunning(processes, "komorebic.exe"))
    windowManager = "Komorebi";

  // Fallback to DWM detection
  if (windowManager.empty()) {
    BOOL compositionEnabled = FALSE;
    if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)))
      windowManager = compositionEnabled ? "DWM" : "Windows Manager (Basic)";
    else
      windowManager = "Windows Manager";
  }

  return windowManager;
}

fn GetDesktopEnvironment() -> optional<string> {
  // Get version information from registry
  const string buildStr =
    GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber");

  DEBUG_LOG("buildStr: {}", buildStr);

  if (buildStr.empty()) {
    DEBUG_LOG("Failed to get CurrentBuildNumber from registry");
    return std::nullopt;
  }

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
  } catch (...) {
    DEBUG_LOG("Failed to parse CurrentBuildNumber");
    return std::nullopt;
  }
}

fn GetShell() -> string {
  // Detect MSYS2/MinGW shells
  if (getenv("MSYSTEM")) {
    const char* shell = getenv("SHELL");
    string      shellExe;

    // First try SHELL, then LOGINSHELL
    if (!shell || strlen(shell) == 0) {
      shell = getenv("LOGINSHELL");
    }

    if (shell) {
      string shellPath = shell;
      size_t lastSlash = shellPath.find_last_of("\\/");
      shellExe         = (lastSlash != string::npos) ? shellPath.substr(lastSlash + 1) : shellPath;
      std::ranges::transform(shellExe, shellExe.begin(), ::tolower);
    }

    // Fallback to process ancestry if both env vars are missing
    if (shellExe.empty()) {
      DWORD pid = GetCurrentProcessId();

      while (pid != 0) {
        string processName = GetProcessName(pid);
        std::ranges::transform(processName, processName.begin(), [](unsigned char character) {
          return static_cast<char>(std::tolower(character));
        });

        if (processName == "bash.exe" || processName == "zsh.exe" || processName == "fish.exe" ||
            processName == "mintty.exe") {
          string name = processName.substr(0, processName.find(".exe"));
          if (!name.empty())
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0]))); // Capitalize first letter
          return name;
        }
        pid = GetParentProcessId(pid);
      }

      return "MSYS2";
    }

    if (shellExe.find("bash") != string::npos)
      return "Bash";
    if (shellExe.find("zsh") != string::npos)
      return "Zsh";
    if (shellExe.find("fish") != string::npos)
      return "Fish";
    return shellExe.empty() ? "MSYS2" : "MSYS2/" + shellExe;
  }

  // Detect Windows shells
  const std::unordered_map<string, string> knownShells = {
    {             "cmd.exe",              "Command Prompt" },
    {      "powershell.exe",                  "PowerShell" },
    {            "pwsh.exe",             "PowerShell Core" },
    { "windowsterminal.exe",            "Windows Terminal" },
    {          "mintty.exe",                      "Mintty" },
    {            "bash.exe", "Windows Subsystem for Linux" }
  };

  DWORD pid = GetCurrentProcessId();
  while (pid != 0) {
    string processName = GetProcessName(pid);
    std::ranges::transform(processName, processName.begin(), ::tolower);

    if (auto shellIterator = knownShells.find(processName); shellIterator != knownShells.end())
      return shellIterator->second;

    pid = GetParentProcessId(pid);
  }

  return "Windows Console";
}

fn GetDiskUsage() -> std::pair<u64, u64> {
  ULARGE_INTEGER freeBytes, totalBytes;

  if (GetDiskFreeSpaceExW(L"C:\\", nullptr, &totalBytes, &freeBytes))
    return { totalBytes.QuadPart - freeBytes.QuadPart, totalBytes.QuadPart };

  return { 0, 0 };
}

#endif
