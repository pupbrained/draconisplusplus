#ifdef _WIN32

// clang-format off
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <dwmapi.h>

#include <cstring>
#include <ranges>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.Diagnostics.h>
#include <winrt/Windows.System.Profile.h>
#include <winrt/base.h>
#include <winrt/impl/Windows.Media.Control.2.h>

#include "src/core/util/error.hpp"
#include "src/core/util/helpers.hpp"
#include "src/core/util/logging.hpp"
#include "src/core/util/types.hpp"

#include "os.hpp"
// clang-format on

namespace {
  using util::error::DraconisError, util::error::DraconisErrorCode;
  using namespace util::types;

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
        error_log("Invalid argument: {}", e.what());
      } catch (const std::out_of_range& e) {
        error_log("Value out of range: {}", e.what());
      } catch (const winrt::hresult_error& e) { error_log("Windows error: {}", winrt::to_string(e.message())); }

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

  fn GetProcessInfo() -> Result<Vec<Pair<DWORD, String>>, DraconisError> {
    try {
      using namespace winrt::Windows::System::Diagnostics;
      using namespace winrt::Windows::Foundation::Collections;

      const IVectorView<ProcessDiagnosticInfo> processInfos = ProcessDiagnosticInfo::GetForProcesses();

      Vec<Pair<DWORD, String>> processes;
      processes.reserve(processInfos.Size());

      for (const auto& processInfo : processInfos)
        processes.emplace_back(processInfo.ProcessId(), winrt::to_string(processInfo.ExecutableFileName()));
      return processes;
    } catch (const winrt::hresult_error& e) { return Err(DraconisError(e)); } catch (const std::exception& e) {
      return Err(DraconisError(e));
    }
  }

  fn IsProcessRunning(const Vec<String>& processNames, const String& name) -> bool {
    return std::ranges::any_of(processNames, [&name](const String& proc) -> bool {
      return _stricmp(proc.c_str(), name.c_str()) == 0;
    });
  }

  template <usize sz>
  fn FindShellInProcessTree(const DWORD startPid, const Array<Pair<StringView, StringView>, sz>& shellMap)
    -> Option<String> {
    if (startPid == 0)
      return None;

    try {
      using namespace winrt::Windows::System::Diagnostics;

      ProcessDiagnosticInfo currentProcessInfo = nullptr;

      try {
        currentProcessInfo = ProcessDiagnosticInfo::TryGetForProcessId(startPid);
      } catch (const winrt::hresult_error& e) {
        error_log("Failed to get process info for PID {}: {}", startPid, winrt::to_string(e.message()));
        return None;
      }

      while (currentProcessInfo) {
        String processName = winrt::to_string(currentProcessInfo.ExecutableFileName());

        if (!processName.empty()) {
          std::ranges::transform(processName, processName.begin(), [](const u8 character) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
          });

          if (processName.length() > 4 && processName.ends_with(".exe"))
            processName.resize(processName.length() - 4);

          auto iter =
            std::ranges::find_if(shellMap, [&](const auto& pair) { return StringView { processName } == pair.first; });

          if (iter != std::ranges::end(shellMap))
            return String { iter->second };
        }

        currentProcessInfo = currentProcessInfo.Parent();
      }
    } catch (const winrt::hresult_error& e) {
      error_log("WinRT error during process tree walk (start PID {}): {}", startPid, winrt::to_string(e.message()));
    } catch (const std::exception& e) {
      error_log("Standard exception during process tree walk (start PID {}): {}", startPid, e.what());
    }

    return None;
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
      debug_log("WinRT error getting build number: {}", winrt::to_string(e.message()));
    } catch (const Exception& e) { debug_log("Standard exception getting build number: {}", e.what()); }

    return None;
  }
} // namespace

fn os::GetMemInfo() -> Result<u64, DraconisError> {
  try {
    return winrt::Windows::System::Diagnostics::SystemDiagnosticInfo::GetForCurrentSystem()
      .MemoryUsage()
      .GetReport()
      .TotalPhysicalSizeInBytes();
  } catch (const winrt::hresult_error& e) { return Err(DraconisError(e)); }
}

fn os::GetNowPlaying() -> Result<MediaInfo, DraconisError> {
  using namespace winrt::Windows::Media::Control;
  using namespace winrt::Windows::Foundation;

  using Session         = GlobalSystemMediaTransportControlsSession;
  using SessionManager  = GlobalSystemMediaTransportControlsSessionManager;
  using MediaProperties = GlobalSystemMediaTransportControlsSessionMediaProperties;

  try {
    const IAsyncOperation<SessionManager> sessionManagerOp = SessionManager::RequestAsync();
    const SessionManager                  sessionManager   = sessionManagerOp.get();

    if (const Session currentSession = sessionManager.GetCurrentSession()) {
      const MediaProperties mediaProperties = currentSession.TryGetMediaPropertiesAsync().get();

      return MediaInfo(
        winrt::to_string(mediaProperties.Title()), winrt::to_string(mediaProperties.Artist()), None, None
      );
    }

    return Err(DraconisError(DraconisErrorCode::NotFound, "No media session found"));
  } catch (const winrt::hresult_error& e) { return Err(DraconisError(e)); }
}

fn os::GetOSVersion() -> Result<String, DraconisError> {
  try {
    const String regSubKey = R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)";

    String       productName    = GetRegistryValue(HKEY_LOCAL_MACHINE, regSubKey, "ProductName");
    const String displayVersion = GetRegistryValue(HKEY_LOCAL_MACHINE, regSubKey, "DisplayVersion");

    if (productName.empty())
      return Err(DraconisError(DraconisErrorCode::NotFound, "ProductName not found in registry"));

    if (const Option<u64> buildNumberOpt = GetBuildNumber()) {
      if (const u64 buildNumber = *buildNumberOpt; buildNumber >= 22000) {
        if (const size_t pos = productName.find("Windows 10"); pos != String::npos) {
          const bool startBoundary = (pos == 0 || !isalnum(static_cast<unsigned char>(productName[pos - 1])));
          const bool endBoundary =
            (pos + 10 == productName.length() || !isalnum(static_cast<unsigned char>(productName[pos + 10])));

          if (startBoundary && endBoundary) {
            productName.replace(pos, 10, "Windows 11");
          }
        }
      }
    } else {
      debug_log("Warning: Could not get build number via WinRT; Win11 detection might be inaccurate.");
    }

    return displayVersion.empty() ? productName : productName + " " + displayVersion;
  } catch (const std::exception& e) { return Err(DraconisError(e)); }
}

fn os::GetHost() -> Result<String, DraconisError> {
  return GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SYSTEM\HardwareConfig\Current)", "SystemFamily");
}

fn os::GetKernelVersion() -> Result<String, DraconisError> {
  try {
    using namespace winrt::Windows::System::Profile;

    const AnalyticsVersionInfo versionInfo = AnalyticsInfo::VersionInfo();

    if (const winrt::hstring familyVersion = versionInfo.DeviceFamilyVersion(); !familyVersion.empty())
      if (auto [major, minor, build, revision] = OSVersion::parseDeviceFamilyVersion(familyVersion); build > 0)
        return std::format("{}.{}.{}.{}", major, minor, build, revision);
  } catch (const winrt::hresult_error& e) { return Err(DraconisError(e)); } catch (const Exception& e) {
    return Err(DraconisError(e));
  }

  return Err(DraconisError(DraconisErrorCode::NotFound, "Could not determine kernel version"));
}

fn os::GetWindowManager() -> Option<String> {
  if (const Result<Vec<Pair<DWORD, String>>, DraconisError> processInfoResult = GetProcessInfo()) {
    const Vec<Pair<DWORD, String>>& processInfo = *processInfoResult;

    Vec<String> processNames;
    processNames.reserve(processInfo.size());

    for (const String& val : processInfo | std::views::values) {
      if (!val.empty()) {
        const usize lastSlash = val.find_last_of("/\\");
        processNames.push_back(lastSlash == String::npos ? val : val.substr(lastSlash + 1));
      }
    }

    const std::unordered_map<String, String> wmProcesses = {
      {   "glazewm.exe",  "GlazeWM" },
      {   "fancywm.exe",  "FancyWM" },
      {  "komorebi.exe", "Komorebi" },
      { "komorebic.exe", "Komorebi" },
    };

    for (const auto& [processExe, wmName] : wmProcesses)
      if (IsProcessRunning(processNames, processExe))
        return wmName;
  } else {
    error_log("Failed to get process info for WM detection: {}", processInfoResult.error().message);
  }

  BOOL compositionEnabled = FALSE;

  if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)))
    return compositionEnabled ? "DWM" : "Windows Manager (Basic)";

  return None;
}

fn os::GetDesktopEnvironment() -> Option<String> {
  const String buildStr =
    GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber");

  if (buildStr.empty()) {
    debug_log("Failed to get CurrentBuildNumber from registry");
    return None;
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
    debug_log("Failed to parse CurrentBuildNumber");
    return None;
  }
}

fn os::GetShell() -> Option<String> {
  using util::helpers::GetEnv;

  try {
    const DWORD currentPid =
      winrt::Windows::System::Diagnostics::ProcessDiagnosticInfo::GetForCurrentProcess().ProcessId();

    if (const Result<String, DraconisError> msystemResult = GetEnv("MSYSTEM");
        msystemResult && !msystemResult->empty()) {
      String shellPath;

      if (const Result<String, DraconisError> shellResult = GetEnv("SHELL"); shellResult && !shellResult->empty())
        shellPath = *shellResult;
      else if (const Result<String, DraconisError> loginShellResult = GetEnv("LOGINSHELL");
               loginShellResult && !loginShellResult->empty())
        shellPath = *loginShellResult;

      if (!shellPath.empty()) {
        const usize lastSlash = shellPath.find_last_of("\\/");
        String      shellExe  = (lastSlash != String::npos) ? shellPath.substr(lastSlash + 1) : shellPath;

        std::ranges::transform(shellExe, shellExe.begin(), [](const u8 character) {
          return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        });

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
  } catch (const winrt::hresult_error& e) {
    error_log("WinRT error during shell detection: {}", winrt::to_string(e.message()));
  } catch (const std::exception& e) { error_log("Standard exception during shell detection: {}", e.what()); }

  return None;
}

fn os::GetDiskUsage() -> Result<DiskSpace, DraconisError> {
  ULARGE_INTEGER freeBytes, totalBytes;

  if (GetDiskFreeSpaceExW(L"C:\\", nullptr, &totalBytes, &freeBytes))
    return DiskSpace { .used_bytes = totalBytes.QuadPart - freeBytes.QuadPart, .total_bytes = totalBytes.QuadPart };

  return Err(DraconisError(util::error::DraconisErrorCode::NotFound, "Failed to get disk usage"));
}

fn os::GetPackageCount() -> Result<u64, DraconisError> {
  return Err(DraconisError(DraconisErrorCode::NotFound, "GetPackageCount not implemented"));
}

#endif
