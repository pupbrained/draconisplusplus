#ifdef _WIN32

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <dwmapi.h>
#include <tlhelp32.h>
// clang-format on

#include <cstring>
#include <ranges>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.Diagnostics.h>
#include <winrt/Windows.System.Profile.h>
#include <winrt/base.h>
#include <winrt/impl/Windows.Media.Control.2.h>

#include "os.h"

namespace {
  struct OSVersion {
    u16 major;
    u16 minor;
    u16 build;
    u16 revision;

    static fn parseDeviceFamilyVersion(const winrt::hstring& versionString) -> OSVersion {
      try {
        const u64 versionUl = std::stoull(winrt::to_string(versionString));
        return {
          .major    = static_cast<u16>((versionUl >> 48) & 0xFFFF),
          .minor    = static_cast<u16>((versionUl >> 32) & 0xFFFF),
          .build    = static_cast<u16>((versionUl >> 16) & 0xFFFF),
          .revision = static_cast<u16>(versionUl & 0xFFFF),
        };
      } catch (const std::invalid_argument& e) {
        ERROR_LOG("Invalid argument: {}", e.what());
      } catch (const std::out_of_range& e) {
        ERROR_LOG("Value out of range: {}", e.what());
      } catch (const winrt::hresult_error& e) { ERROR_LOG("Windows error: {}", winrt::to_string(e.message())); }

      return { .major = 0, .minor = 0, .build = 0, .revision = 0 };
    }
  };

  // clang-format off
  constexpr Array<Pair<StringView, StringView>, 3> windowsShellMap = {{
    {        "cmd",  "Command Prompt" },
    { "powershell",      "PowerShell" },
    {       "pwsh", "PowerShell Core" },
  }};

  constexpr Array<Pair<StringView, StringView>, 3> msysShellMap = {{
    { "bash", "Bash" },
    {  "zsh",  "Zsh" },
    { "fish", "Fish" },
  }};
  // clang-format on

  class ProcessSnapshot {
   public:
    ProcessSnapshot() : h_snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)) {}

    ProcessSnapshot(ProcessSnapshot&&)                     = delete;
    ProcessSnapshot(const ProcessSnapshot&)                = delete;
    fn operator=(ProcessSnapshot&&)->ProcessSnapshot&      = delete;
    fn operator=(const ProcessSnapshot&)->ProcessSnapshot& = delete;

    ~ProcessSnapshot() {
      if (h_snapshot != INVALID_HANDLE_VALUE)
        CloseHandle(h_snapshot);
    }

    [[nodiscard]] fn isValid() const -> bool { return h_snapshot != INVALID_HANDLE_VALUE; }

    [[nodiscard]] fn getProcesses() const -> Vec<Pair<DWORD, String>> {
      Vec<Pair<DWORD, String>> processes;

      if (!isValid())
        return processes;

      PROCESSENTRY32 pe32;
      pe32.dwSize = sizeof(PROCESSENTRY32);

      if (!Process32First(h_snapshot, &pe32))
        return processes;

      if (Process32First(h_snapshot, &pe32)) {
        processes.emplace_back(pe32.th32ProcessID, String(reinterpret_cast<const char*>(pe32.szExeFile)));

        while (Process32Next(h_snapshot, &pe32))
          processes.emplace_back(pe32.th32ProcessID, String(reinterpret_cast<const char*>(pe32.szExeFile)));
      }

      return processes;
    }

    HANDLE h_snapshot;
  };

  fn GetRegistryValue(const HKEY& hKey, const String& subKey, const String& valueName) -> String {
    HKEY key = nullptr;
    if (RegOpenKeyExA(hKey, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
      return "";

    DWORD dataSize = 0;
    DWORD type     = 0;
    if (RegQueryValueExA(key, valueName.c_str(), nullptr, &type, nullptr, &dataSize) != ERROR_SUCCESS) {
      RegCloseKey(key);
      return "";
    }

    String value((type == REG_SZ || type == REG_EXPAND_SZ) ? dataSize - 1 : dataSize, '\0');

    if (RegQueryValueExA(key, valueName.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(value.data()), &dataSize) !=
        ERROR_SUCCESS) {
      RegCloseKey(key);
      return "";
    }

    RegCloseKey(key);
    return value;
  }

  fn GetProcessInfo() -> Vec<Pair<DWORD, String>> {
    const ProcessSnapshot snapshot;
    return snapshot.isValid() ? snapshot.getProcesses() : std::vector<std::pair<DWORD, String>> {};
  }

  fn IsProcessRunning(const Vec<String>& processes, const String& name) -> bool {
    return std::ranges::any_of(processes, [&name](const String& proc) -> bool {
      return _stricmp(proc.c_str(), name.c_str()) == 0;
    });
  }

  fn GetParentProcessId(const DWORD pid) -> DWORD {
    const ProcessSnapshot snapshot;
    if (!snapshot.isValid())
      return 0;

    PROCESSENTRY32 pe32 { .dwSize = sizeof(PROCESSENTRY32) };

    if (!Process32First(snapshot.h_snapshot, &pe32))
      return 0;

    if (pe32.th32ProcessID == pid)
      return pe32.th32ParentProcessID;

    while (Process32Next(snapshot.h_snapshot, &pe32))
      if (pe32.th32ProcessID == pid)
        return pe32.th32ParentProcessID;

    return 0;
  }

  fn GetProcessName(const DWORD pid) -> String {
    const ProcessSnapshot snapshot;
    if (!snapshot.isValid())
      return "";

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(snapshot.h_snapshot, &pe32))
      return "";

    if (pe32.th32ProcessID == pid)
      return reinterpret_cast<const char*>(pe32.szExeFile);

    while (Process32Next(snapshot.h_snapshot, &pe32))
      if (pe32.th32ProcessID == pid)
        return reinterpret_cast<const char*>(pe32.szExeFile);

    return "";
  }

  template <usize sz>
  fn FindShellInProcessTree(const DWORD startPid, const Array<Pair<StringView, StringView>, sz>& shellMap)
    -> std::optional<String> {
    DWORD pid = startPid;
    while (pid != 0) {
      String processName = GetProcessName(pid);

      if (processName.empty()) {
        pid = GetParentProcessId(pid);
        continue;
      }

      std::ranges::transform(processName, processName.begin(), [](const u8 character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
      });

      if (processName.length() > 4 && processName.substr(processName.length() - 4) == ".exe")
        processName.resize(processName.length() - 4);

      auto iter = std::ranges::find_if(shellMap, [&](const auto& pair) {
        return std::string_view { processName } == pair.first;
      });

      if (iter != std::ranges::end(shellMap))
        return String { iter->second };

      pid = GetParentProcessId(pid);
    }

    return std::nullopt;
  }

  fn GetBuildNumber() -> Option<u64> {
    try {
      using namespace winrt::Windows::System::Profile;
      const auto           versionInfo   = AnalyticsInfo::VersionInfo();
      const winrt::hstring familyVersion = versionInfo.DeviceFamilyVersion();

      if (!familyVersion.empty()) {
        const u64 versionUl = std::stoull(winrt::to_string(familyVersion));
        return (versionUl >> 16) & 0xFFFF;
      }
    } catch (const winrt::hresult_error& e) {
      DEBUG_LOG("WinRT error getting build number: {}", winrt::to_string(e.message()));
    } catch (const Exception& e) { DEBUG_LOG("Standard exception getting build number: {}", e.what()); }

    return None;
  }
}

fn os::GetMemInfo() -> Result<u64, String> {
  try {
    return winrt::Windows::System::Diagnostics::SystemDiagnosticInfo::GetForCurrentSystem()
      .MemoryUsage()
      .GetReport()
      .TotalPhysicalSizeInBytes();
  } catch (const winrt::hresult_error& e) {
    return Err(std::format("Failed to get memory info: {}", to_string(e.message())));
  }
}

fn os::GetNowPlaying() -> Result<String, NowPlayingError> {
  using namespace winrt::Windows::Media::Control;
  using namespace winrt::Windows::Foundation;

  using Session        = GlobalSystemMediaTransportControlsSession;
  using SessionManager = GlobalSystemMediaTransportControlsSessionManager;

  try {
    const IAsyncOperation<SessionManager> sessionManagerOp = SessionManager::RequestAsync();
    const SessionManager                  sessionManager   = sessionManagerOp.get();

    if (const Session currentSession = sessionManager.GetCurrentSession())
      return winrt::to_string(currentSession.TryGetMediaPropertiesAsync().get().Title());

    return Err(NowPlayingCode::NoActivePlayer);
  } catch (const winrt::hresult_error& e) { return Err(e); }
}

fn os::GetOSVersion() -> Result<String, String> {
  try {
    const String regSubKey = R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)";

    String       productName    = GetRegistryValue(HKEY_LOCAL_MACHINE, regSubKey, "ProductName");
    const String displayVersion = GetRegistryValue(HKEY_LOCAL_MACHINE, regSubKey, "DisplayVersion");

    if (productName.empty())
      return Err("Failed to read ProductName");

    if (const Option<u64> buildNumber = GetBuildNumber()) {
      if (*buildNumber >= 22000)
        if (const usize pos = productName.find("Windows 10");
            pos != String::npos && (pos == 0 || !isalnum(static_cast<u8>(productName[pos - 1]))) &&
            (pos + 10 == productName.length() || !isalnum(static_cast<u8>(productName[pos + 10]))))
          productName.replace(pos, 10, "Windows 11");
    } else
      DEBUG_LOG("Warning: Could not get build number via WinRT; Win11 patch relies on registry ProductName only.");

    return displayVersion.empty() ? productName : productName + " " + displayVersion;
  } catch (const Exception& e) { return Err(std::format("Exception occurred getting OS version: {}", e.what())); }
}

fn os::GetHost() -> String {
  return GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SYSTEM\HardwareConfig\Current)", "SystemFamily");
}

fn os::GetKernelVersion() -> String {
  try {
    using namespace winrt::Windows::System::Profile;

    const AnalyticsVersionInfo versionInfo = AnalyticsInfo::VersionInfo();

    if (const winrt::hstring familyVersion = versionInfo.DeviceFamilyVersion(); !familyVersion.empty())
      if (auto [major, minor, build, revision] = OSVersion::parseDeviceFamilyVersion(familyVersion); build > 0)
        return std::format("{}.{}.{}.{}", major, minor, build, revision);
  } catch (const winrt::hresult_error& e) {
    ERROR_LOG("WinRT error: {}", winrt::to_string(e.message()));
  } catch (const Exception& e) { ERROR_LOG("Failed to get kernel version: {}", e.what()); }

  return "";
}

fn os::GetWindowManager() -> String {
  const auto          processInfo = GetProcessInfo();
  std::vector<String> processNames;

  processNames.reserve(processInfo.size());
  for (const auto& name : processInfo | std::views::values) processNames.push_back(name);

  const std::unordered_map<String, String> wmProcesses = {
    {   "glazewm.exe",  "GlazeWM" },
    {   "fancywm.exe",  "FancyWM" },
    {  "komorebi.exe", "Komorebi" },
    { "komorebic.exe", "Komorebi" }
  };

  for (const auto& [processName, wmName] : wmProcesses)
    if (IsProcessRunning(processNames, processName))
      return wmName;

  BOOL compositionEnabled = FALSE;
  if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)))
    return compositionEnabled ? "DWM" : "Windows Manager (Basic)";

  return "Windows Manager";
}

fn os::GetDesktopEnvironment() -> Option<String> {
  const String buildStr =
    GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber");

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
      const String productName =
        GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "ProductName");

      if (productName.find("Windows 10") != String::npos)
        return "Metro (Windows 10)";

      if (build >= 9600)
        return "Metro (Windows 8.1)";

      return "Metro (Windows 8)";
    }

    // Windows 7 Aero
    if (build >= 7600)
      return "Aero (Windows 7)";

    // Pre-Win7
    return "Classic";
  } catch (...) {
    DEBUG_LOG("Failed to parse CurrentBuildNumber");
    return std::nullopt;
  }
}

fn os::GetShell() -> String {
  const DWORD currentPid = GetCurrentProcessId();

  if (const Result<String, EnvError> msystemResult = GetEnv("MSYSTEM")) {
    String shellPath;
    if (const Result<String, EnvError> shellResult = GetEnv("SHELL"); !shellResult->empty())
      shellPath = *shellResult;
    else if (const Result<String, EnvError> loginShellResult = GetEnv("LOGINSHELL"); !loginShellResult->empty())
      shellPath = *loginShellResult;

    if (!shellPath.empty()) {
      const usize lastSlash = shellPath.find_last_of("\\/");
      String      shellExe  = (lastSlash != String::npos) ? shellPath.substr(lastSlash + 1) : shellPath;

      std::ranges::transform(shellExe, shellExe.begin(), [](const u8 c) { return std::tolower(c); });

      if (shellExe.ends_with(".exe"))
        shellExe.resize(shellExe.length() - 4);

      const auto iter =
        std::ranges::find_if(msysShellMap, [&](const auto& pair) { return StringView { shellExe } == pair.first; });

      if (iter != std::ranges::end(msysShellMap))
        return String { iter->second };
    }

    if (const Option<String> msysShell = FindShellInProcessTree(currentPid, msysShellMap))
      return *msysShell;

    return "MSYS2 Environment";
  }

  if (const Option<String> windowsShell = FindShellInProcessTree(currentPid, windowsShellMap))
    return *windowsShell;

  return "Unknown Shell";
}

fn os::GetDiskUsage() -> Pair<u64, u64> {
  ULARGE_INTEGER freeBytes, totalBytes;

  if (GetDiskFreeSpaceExW(L"C:\\", nullptr, &totalBytes, &freeBytes))
    return { totalBytes.QuadPart - freeBytes.QuadPart, totalBytes.QuadPart };

  return { 0, 0 };
}

#endif
