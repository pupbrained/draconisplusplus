#ifdef _WIN32

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <dwmapi.h>
#include <tlhelp32.h>
// clang-format on

#include <algorithm>
#include <cstring>
#include <guiddef.h>
#include <ranges>
#include <vector>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.Diagnostics.h>
#include <winrt/base.h>
#include <winrt/impl/Windows.Media.Control.2.h>

#include "os.h"

using std::string_view;
using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

// NOLINTBEGIN(*-pro-type-cstyle-cast,*-no-int-to-ptr,*-pro-type-reinterpret-cast)
namespace {
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

    [[nodiscard]] fn getProcesses() const -> std::vector<std::pair<DWORD, string>> {
      std::vector<std::pair<DWORD, string>> processes;

      if (!isValid())
        return processes;

      PROCESSENTRY32 pe32;
      pe32.dwSize = sizeof(PROCESSENTRY32);

      if (!Process32First(h_snapshot, &pe32))
        return processes;

      // Get first process
      if (Process32First(h_snapshot, &pe32)) {
        // Add first process to vector
        processes.emplace_back(pe32.th32ProcessID, string(reinterpret_cast<const char*>(pe32.szExeFile)));

        // Add remaining processes
        while (Process32Next(h_snapshot, &pe32))
          processes.emplace_back(pe32.th32ProcessID, string(reinterpret_cast<const char*>(pe32.szExeFile)));
      }

      return processes;
    }

    HANDLE h_snapshot;
  };

  fn GetRegistryValue(const HKEY& hKey, const string& subKey, const string& valueName) -> string {
    HKEY key = nullptr;
    if (RegOpenKeyExA(hKey, subKey.c_str(), 0, KEY_READ, &key) != ERROR_SUCCESS)
      return "";

    DWORD dataSize = 0;
    DWORD type     = 0;
    if (RegQueryValueExA(key, valueName.c_str(), nullptr, &type, nullptr, &dataSize) != ERROR_SUCCESS) {
      RegCloseKey(key);
      return "";
    }

    // For string values, allocate one less byte to avoid the null terminator
    string value((type == REG_SZ || type == REG_EXPAND_SZ) ? dataSize - 1 : dataSize, '\0');

    if (RegQueryValueExA(key, valueName.c_str(), nullptr, nullptr, std::bit_cast<LPBYTE>(value.data()), &dataSize) !=
        ERROR_SUCCESS) {
      RegCloseKey(key);
      return "";
    }

    RegCloseKey(key);
    return value;
  }

  fn GetProcessInfo() -> std::vector<std::pair<DWORD, string>> {
    const ProcessSnapshot snapshot;
    return snapshot.isValid() ? snapshot.getProcesses() : std::vector<std::pair<DWORD, string>> {};
  }

  fn IsProcessRunning(const std::vector<string>& processes, const string& name) -> bool {
    return std::ranges::any_of(processes, [&name](const string& proc) -> bool {
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

  fn GetProcessName(const DWORD pid) -> string {
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
}

fn GetMemInfo() -> expected<u64, string> {
  try {
    using namespace winrt::Windows::System::Diagnostics;
    const SystemDiagnosticInfo diag = SystemDiagnosticInfo::GetForCurrentSystem();
    return diag.MemoryUsage().GetReport().TotalPhysicalSizeInBytes();
  } catch (const winrt::hresult_error& e) {
    return std::unexpected("Failed to get memory info: " + to_string(e.message()));
  }
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
  // First try using the native Windows API
  constexpr OSVERSIONINFOEXW osvi   = { sizeof(OSVERSIONINFOEXW), 0, 0, 0, 0, { 0 }, 0, 0, 0, 0, 0 };
  NTSTATUS                   status = 0;

  // Get RtlGetVersion function from ntdll.dll (not affected by application manifest)
  if (const HMODULE ntdllHandle = GetModuleHandleW(L"ntdll.dll"))
    if (const auto rtlGetVersion = std::bit_cast<RtlGetVersionPtr>(GetProcAddress(ntdllHandle, "RtlGetVersion")))
      status = rtlGetVersion(std::bit_cast<PRTL_OSVERSIONINFOW>(&osvi));

  string productName;
  string edition;

  if (status == 0) { // STATUS_SUCCESS
    // We need to get the edition information which isn't available from version API
    // Use GetProductInfo which is available since Vista
    DWORD productType = 0;
    if (GetProductInfo(
          osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.wServicePackMajor, osvi.wServicePackMinor, &productType
        )) {
      if (osvi.dwMajorVersion == 10) {
        if (osvi.dwBuildNumber >= 22000) {
          productName = "Windows 11";
        } else {
          productName = "Windows 10";
        }

        switch (productType) {
          case PRODUCT_PROFESSIONAL:
            edition = " Pro";
            break;
          case PRODUCT_ENTERPRISE:
            edition = " Enterprise";
            break;
          case PRODUCT_EDUCATION:
            edition = " Education";
            break;
          case PRODUCT_HOME_BASIC:
          case PRODUCT_HOME_PREMIUM:
            edition = " Home";
            break;
          case PRODUCT_CLOUDEDITION:
            edition = " Cloud";
            break;
          default:
            break;
        }
      }
    }
  } else {
    // Fallback to registry method if the API approach fails
    productName =
      GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "ProductName");

    // Check for Windows 11
    if (const i32 buildNumber = stoi(
          GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber")
        );
        buildNumber >= 22000 && productName.find("Windows 10") != string::npos)
      productName.replace(productName.find("Windows 10"), 10, "Windows 11");
  }

  if (!productName.empty()) {
    string result = productName + edition;

    const string displayVersion =
      GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "DisplayVersion");

    if (!displayVersion.empty())
      result += " " + displayVersion;

    return result;
  }

  return "Windows";
}

fn GetHost() -> string {
  string hostName = GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SYSTEM\HardwareConfig\Current)", "SystemFamily");

  return hostName;
}

fn GetKernelVersion() -> string {
  // ReSharper disable once CppLocalVariableMayBeConst
  if (HMODULE ntdllHandle = GetModuleHandleW(L"ntdll.dll")) {
    if (const auto rtlGetVersion = std::bit_cast<RtlGetVersionPtr>(GetProcAddress(ntdllHandle, "RtlGetVersion"))) {
      RTL_OSVERSIONINFOW osInfo  = {};
      osInfo.dwOSVersionInfoSize = sizeof(osInfo);

      if (rtlGetVersion(&osInfo) == 0) {
        return std::format(
          "{}.{}.{}.{}", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.dwPlatformId
        );
      }
    }
  }

  return "";
}

fn GetWindowManager() -> string {
  // Get process information once and reuse it
  const auto          processInfo = GetProcessInfo();
  std::vector<string> processNames;

  processNames.reserve(processInfo.size());
  for (const auto& name : processInfo | std::views::values) processNames.push_back(name);

  // Check for third-party WMs using a map for cleaner code
  const std::unordered_map<string, string> wmProcesses = {
    {   "glazewm.exe",  "GlazeWM" },
    {   "fancywm.exe",  "FancyWM" },
    {  "komorebi.exe", "Komorebi" },
    { "komorebic.exe", "Komorebi" }
  };

  for (const auto& [processName, wmName] : wmProcesses) {
    if (IsProcessRunning(processNames, processName))
      return wmName;
  }

  // Fallback to DWM detection
  BOOL compositionEnabled = FALSE;
  if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)))
    return compositionEnabled ? "DWM" : "Windows Manager (Basic)";

  return "Windows Manager";
}

fn GetDesktopEnvironment() -> optional<string> {
  // Get version information from registry
  const string buildStr =
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
  // Define known shells map once for reuse
  const std::unordered_map<string, string> knownShells = {
    {             "cmd.exe",              "Command Prompt" },
    {      "powershell.exe",                  "PowerShell" },
    {            "pwsh.exe",             "PowerShell Core" },
    { "windowsterminal.exe",            "Windows Terminal" },
    {          "mintty.exe",                      "Mintty" },
    {            "bash.exe", "Windows Subsystem for Linux" }
  };

  // Detect MSYS2/MinGW shells
  char* msystemEnv = nullptr;
  if (_dupenv_s(&msystemEnv, nullptr, "MSYSTEM") == 0 && msystemEnv != nullptr) {
    const std::unique_ptr<char, decltype(&free)> msystemEnvGuard(msystemEnv, free);

    // Get shell from environment variables
    char*  shell    = nullptr;
    size_t shellLen = 0;
    _dupenv_s(&shell, &shellLen, "SHELL");
    const std::unique_ptr<char, decltype(&free)> shellGuard(shell, free);

    // If SHELL is empty, try LOGINSHELL
    if (!shell || strlen(shell) == 0) {
      char*  loginShell    = nullptr;
      size_t loginShellLen = 0;
      _dupenv_s(&loginShell, &loginShellLen, "LOGINSHELL");
      const std::unique_ptr<char, decltype(&free)> loginShellGuard(loginShell, free);
      shell = loginShell;
    }

    if (shell) {
      string       shellExe;
      const string shellPath = shell;
      const size_t lastSlash = shellPath.find_last_of("\\/");
      shellExe               = (lastSlash != string::npos) ? shellPath.substr(lastSlash + 1) : shellPath;
      std::ranges::transform(shellExe, shellExe.begin(), ::tolower);

      // Use a map for shell name lookup instead of multiple if statements
      const std::unordered_map<string_view, string> shellNames = {
        { "bash", "Bash" },
        {  "zsh",  "Zsh" },
        { "fish", "Fish" }
      };

      for (const auto& [pattern, name] : shellNames) {
        if (shellExe.find(pattern) != string::npos)
          return name;
      }

      return shellExe.empty() ? "MSYS2" : "MSYS2/" + shellExe;
    }

    // Fallback to process ancestry with cached process info
    const auto processInfo = GetProcessInfo();
    DWORD      pid         = GetCurrentProcessId();

    while (pid != 0) {
      string processName = GetProcessName(pid);
      std::ranges::transform(processName, processName.begin(), ::tolower);

      const std::unordered_map<string, string> msysShells = {
        {   "bash.exe",   "Bash" },
        {    "zsh.exe",    "Zsh" },
        {   "fish.exe",   "Fish" },
        { "mintty.exe", "Mintty" }
      };

      for (const auto& [msysShellExe, shellName] : msysShells) {
        if (processName == msysShellExe)
          return shellName;
      }

      pid = GetParentProcessId(pid);
    }

    return "MSYS2";
  }

  // Detect Windows shells
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
// NOLINTEND(*-pro-type-cstyle-cast,*-no-int-to-ptr,*-pro-type-reinterpret-cast)

#endif
