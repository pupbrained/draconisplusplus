#ifdef _WIN32

// clang-format off
#include <dwmapi.h>
#include <ranges>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.System.Profile.h>

#if DRAC_ENABLE_PACKAGECOUNT
  #include "Services/PackageCounting.hpp"
#endif

#include "Util/Env.hpp"
#include "Util/Error.hpp"
#include "Util/Logging.hpp"
#include "Util/Types.hpp"

#include "Core/System.hpp"
// clang-format on

using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

namespace {
  using util::error::DracError, util::error::DracErrorCode;
  using namespace util::types;

  struct ProcessData {
    DWORD  parentPid = 0;
    String baseExeNameLower;
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

    // NOLINTNEXTLINE(*-pro-type-reinterpret-cast) - required here
    if (RegQueryValueExA(key, valueName.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(value.data()), &dataSize) != ERROR_SUCCESS) {
      RegCloseKey(key);
      return "";
    }

    RegCloseKey(key);
    return value;
  }

  template <usize sz>
  fn FindShellInProcessTree(const DWORD startPid, const Array<Pair<StringView, StringView>, sz>& shellMap) -> Result<String> {
    if (startPid == 0)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Start PID is 0"));

    std::unordered_map<DWORD, ProcessData> processMap;

    // ReSharper disable once CppLocalVariableMayBeConst
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnap == INVALID_HANDLE_VALUE) {
      return Err(DracError(DracErrorCode::PlatformSpecific, std::format("Failed snapshot, error {}", GetLastError())));
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe32)) {
      // NOLINTNEXTLINE(*-avoid-do-while)
      do {
        const String fullName = pe32.szExeFile;
        String       baseName;

        const size_t lastSlash = fullName.find_last_of("\\/");

        baseName = (lastSlash == String::npos) ? fullName : fullName.substr(lastSlash + 1);

        std::transform(baseName.begin(), baseName.end(), baseName.begin(), [](const u8 character) {
          return std::tolower(character);
        });

        if (baseName.length() > 4 && baseName.ends_with(".exe"))
          baseName.resize(baseName.length() - 4);

        processMap[pe32.th32ProcessID] =
          ProcessData { .parentPid = pe32.th32ParentProcessID, .baseExeNameLower = std::move(baseName) };
      } while (Process32Next(hSnap, &pe32));
    } else
      return Err(DracError(DracErrorCode::PlatformSpecific, std::format("Process32First failed, error {}", GetLastError())));

    CloseHandle(hSnap);

    DWORD currentPid = startPid;

    i32 depth = 0;

    constexpr int maxDepth = 32;

    while (currentPid != 0 && depth < maxDepth) {
      auto procIt = processMap.find(currentPid);

      if (procIt == processMap.end())
        break;

      const String& processName = procIt->second.baseExeNameLower;

      auto mapIter =
        std::ranges::find_if(shellMap, [&](const auto& pair) { return StringView { processName } == pair.first; });

      if (mapIter != std::ranges::end(shellMap))
        return String { mapIter->second };

      currentPid = procIt->second.parentPid;

      depth++;
    }

    if (depth >= maxDepth)
      return Err(DracError(DracErrorCode::PlatformSpecific, std::format("Reached max depth limit ({}) walking parent PIDs from {}", maxDepth, startPid)));

    return Err(DracError(DracErrorCode::NotFound, "Shell not found"));
  }

  fn GetBuildNumber() -> Result<u64> {
    try {
      using namespace winrt::Windows::System::Profile;
      const AnalyticsVersionInfo versionInfo   = AnalyticsInfo::VersionInfo();
      const winrt::hstring       familyVersion = versionInfo.DeviceFamilyVersion();

      if (!familyVersion.empty()) {
        const u64 versionUl = std::stoull(winrt::to_string(familyVersion));
        return (versionUl >> 16) & 0xFFFF;
      }
    } catch (const winrt::hresult_error& e) {
      return Err(DracError(e));
    } catch (const Exception& e) { return Err(DracError(e)); }

    return Err(DracError(DracErrorCode::NotFound, "Failed to get build number"));
  }
} // namespace

namespace os {
  fn System::getMemInfo() -> Result<ResourceUsage> {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo))
      return ResourceUsage { .usedBytes = memInfo.ullTotalPhys - memInfo.ullAvailPhys, .totalBytes = memInfo.ullTotalPhys };

    return Err(DracError(DracErrorCode::PlatformSpecific, std::format("GlobalMemoryStatusEx failed with error code {}", GetLastError())));
  }

  #if DRAC_ENABLE_NOWPLAYING
  fn System::getNowPlaying() -> Result<MediaInfo> {
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

        return MediaInfo(winrt::to_string(mediaProperties.Title()), winrt::to_string(mediaProperties.Artist()));
      }

      return Err(DracError(DracErrorCode::NotFound, "No media session found"));
    } catch (const winrt::hresult_error& e) { return Err(DracError(e)); }
  }
  #endif // DRAC_ENABLE_NOWPLAYING

  fn System::getOSVersion() -> Result<String> {
    try {
      const String regSubKey = R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)";

      String       productName    = GetRegistryValue(HKEY_LOCAL_MACHINE, regSubKey, "ProductName");
      const String displayVersion = GetRegistryValue(HKEY_LOCAL_MACHINE, regSubKey, "DisplayVersion");

      if (productName.empty())
        return Err(DracError(DracErrorCode::NotFound, "ProductName not found in registry"));

      if (const Result<u64> buildNumberOpt = GetBuildNumber()) {
        if (const u64 buildNumber = *buildNumberOpt; buildNumber >= 22000) {
          if (const size_t pos = productName.find("Windows 10"); pos != String::npos) {
            const bool startBoundary = (pos == 0 || !isalnum(static_cast<u8>(productName[pos - 1])));
            // ReSharper disable once CppTooWideScopeInitStatement
            const bool endBoundary = (pos + 10 == productName.length() || !isalnum(static_cast<u8>(productName[pos + 10])));

            if (startBoundary && endBoundary)
              productName.replace(pos, 10, "Windows 11");
          }
        }
      } else {
        debug_at(buildNumberOpt.error());
      }

      return displayVersion.empty() ? productName : productName + " " + displayVersion;
    } catch (const std::exception& e) { return Err(DracError(e)); }
  }

  fn System::getHost() -> Result<String> {
    return GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SYSTEM\HardwareConfig\Current)", "SystemFamily");
  }

  fn System::getKernelVersion() -> Result<String> {
    if (const HMODULE ntdllHandle = GetModuleHandleW(L"ntdll.dll")) {
      // NOLINTNEXTLINE(*-pro-type-reinterpret-cast) - required here
      if (const auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdllHandle, "RtlGetVersion"))) {
        RTL_OSVERSIONINFOW osInfo  = {};
        osInfo.dwOSVersionInfoSize = sizeof(osInfo);

        if (rtlGetVersion(&osInfo) == 0)
          return std::format("{}.{}.{}.{}", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.dwPlatformId);
      }
    }

    return Err(DracError(DracErrorCode::NotFound, "Could not determine kernel version using RtlGetVersion"));
  }

  fn System::getWindowManager() -> Result<String> {
    BOOL compositionEnabled = FALSE;

    if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)))
      return compositionEnabled ? "DWM" : "Windows Manager (Basic)";

    return Err(DracError(DracErrorCode::NotFound, "Failed to get window manager (DwmIsCompositionEnabled failed"));
  }

  fn System::getDesktopEnvironment() -> Result<String> {
    const String buildStr =
      GetRegistryValue(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", "CurrentBuildNumber");

    if (buildStr.empty())
      return Err(DracError(DracErrorCode::InternalError, "Failed to get CurrentBuildNumber from registry"));

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
    } catch (...) { return Err(DracError(DracErrorCode::ParseError, "Failed to parse CurrentBuildNumber")); }
  }

  fn System::getShell() -> Result<String> {
    using util::helpers::GetEnv;

    if (const Result<String> msystemResult = GetEnv("MSYSTEM"); msystemResult && !msystemResult->empty()) {
      String shellPath;

      if (const Result<String> shellResult = GetEnv("SHELL"); shellResult && !shellResult->empty())
        shellPath = *shellResult;
      else if (const Result<String> loginShellResult = GetEnv("LOGINSHELL");
               loginShellResult && !loginShellResult->empty())
        shellPath = *loginShellResult;

      if (!shellPath.empty()) {
        const usize lastSlash = shellPath.find_last_of("\\/");
        String      shellExe  = (lastSlash != String::npos) ? shellPath.substr(lastSlash + 1) : shellPath;
        std::ranges::transform(shellExe, shellExe.begin(), [](const u8 character) { return std::tolower(character); });
        if (shellExe.ends_with(".exe"))
          shellExe.resize(shellExe.length() - 4);

        const auto iter =
          std::ranges::find_if(msysShellMap, [&](const auto& pair) { return StringView { shellExe } == pair.first; });
        if (iter != std::ranges::end(msysShellMap))
          return String { iter->second };
      }

      const DWORD          currentPid = GetCurrentProcessId();
      const Result<String> msysShell  = FindShellInProcessTree(currentPid, msysShellMap);

      if (msysShell)
        return *msysShell;

      return Err(msysShell.error());
    }

    const DWORD currentPid = GetCurrentProcessId();

    if (const Result<String> windowsShell = FindShellInProcessTree(currentPid, windowsShellMap))
      return *windowsShell;

    return Err(DracError(DracErrorCode::NotFound, "Shell not found"));
  }

  fn System::getDiskUsage() -> Result<ResourceUsage> {
    ULARGE_INTEGER freeBytes, totalBytes;

    if (GetDiskFreeSpaceExW(L"C:\\\\", nullptr, &totalBytes, &freeBytes))
      return ResourceUsage { .usedBytes = totalBytes.QuadPart - freeBytes.QuadPart, .totalBytes = totalBytes.QuadPart };

    return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get disk usage"));
  }

  fn System::getCPUModel() -> Result<String> {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to open registry key for CPU information"));

    DWORD dataSize = 0;
    DWORD type     = 0;

    if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, &type, nullptr, &dataSize) != ERROR_SUCCESS) {
      RegCloseKey(hKey);
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to read CPU model size from registry"));
    }

    std::wstring processorName(dataSize / sizeof(wchar_t), '\0');

    // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
    if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, reinterpret_cast<LPBYTE>(processorName.data()), &dataSize) != ERROR_SUCCESS) {
      RegCloseKey(hKey);
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to read CPU model from registry"));
    }

    RegCloseKey(hKey);

    String utf8Name;
    utf8Name.reserve(processorName.length());
    for (wchar_t wchar : processorName) {
      if (wchar == 0)
        break;
      if (wchar < 0x80) {
        utf8Name.push_back(static_cast<char>(wchar));
      } else if (wchar < 0x800) {
        utf8Name.push_back(static_cast<char>(0xC0 | (wchar >> 6)));
        utf8Name.push_back(static_cast<char>(0x80 | (wchar & 0x3F)));
      } else {
        utf8Name.push_back(static_cast<char>(0xE0 | (wchar >> 12)));
        utf8Name.push_back(static_cast<char>(0x80 | ((wchar >> 6) & 0x3F)));
        utf8Name.push_back(static_cast<char>(0x80 | (wchar & 0x3F)));
      }
    }

    return utf8Name;
  }

  fn System::getGPUModel() -> Result<String> {
    HDEVINFO hdev = SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY, nullptr, nullptr, DIGCF_PRESENT);
    if (hdev == INVALID_HANDLE_VALUE)
      return Err(DracError(DracErrorCode::PlatformSpecific, "SetupDiGetClassDevsW failed"));

    SP_DEVINFO_DATA did;
    did.cbSize = sizeof(did);

    String gpuName;

    if (SetupDiEnumDeviceInfo(hdev, 0, &did)) {
      Array<wchar_t, 256> buffer;
      DWORD               bufferLen = buffer.size();

      // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
      if (SetupDiGetDeviceRegistryPropertyW(hdev, &did, SPDRP_DEVICEDESC, nullptr, reinterpret_cast<PBYTE>(buffer.data()), buffer.size(), &bufferLen)) {
        gpuName.reserve(bufferLen / sizeof(wchar_t));
        for (wchar_t wchar : std::wstring_view(buffer.data(), bufferLen / sizeof(wchar_t))) {
          if (wchar == 0)
            break;
          if (wchar < 0x80) {
            gpuName.push_back(static_cast<char>(wchar));
          } else if (wchar < 0x800) {
            gpuName.push_back(static_cast<char>(0xC0 | (wchar >> 6)));
            gpuName.push_back(static_cast<char>(0x80 | (wchar & 0x3F)));
          } else {
            gpuName.push_back(static_cast<char>(0xE0 | (wchar >> 12)));
            gpuName.push_back(static_cast<char>(0x80 | ((wchar >> 6) & 0x3F)));
            gpuName.push_back(static_cast<char>(0x80 | (wchar & 0x3F)));
          }
        }
      }
    }

    SetupDiDestroyDeviceInfoList(hdev);

    if (gpuName.empty())
      return Err(DracError(DracErrorCode::NotFound, "No GPU found"));

    return gpuName;
  }
} // namespace os

  #if DRAC_ENABLE_PACKAGECOUNT
namespace package {
  using util::helpers::GetEnv;

  fn CountChocolatey() -> Result<u64> {
    const fs::path chocoPath = fs::path(GetEnv("ChocolateyInstall").value_or("C:\\ProgramData\\chocolatey")) / "lib";

    if (!fs::exists(chocoPath) || !fs::is_directory(chocoPath))
      return Err(DracError(DracErrorCode::NotFound, std::format("Chocolatey directory not found: {}", chocoPath.string())));

    return GetCountFromDirectory("Chocolatey", chocoPath);
  }

  fn CountScoop() -> Result<u64> {
    fs::path scoopAppsPath;

    if (const Result<String> scoopEnvPath = GetEnv("SCOOP"))
      scoopAppsPath = fs::path(*scoopEnvPath) / "apps";
    else if (const Result<String> userProfilePath = GetEnv("USERPROFILE"))
      scoopAppsPath = fs::path(*userProfilePath) / "scoop" / "apps";
    else
      return Err(DracError(
        DracErrorCode::NotFound,
        "Could not determine Scoop installation directory (SCOOP and USERPROFILE environment variables not found)"
      ));

    return GetCountFromDirectory("Scoop", scoopAppsPath, true);
  }

  fn CountWinGet() -> Result<u64> {
    try {
      return std::ranges::distance(winrt::Windows::Management::Deployment::PackageManager().FindPackagesForUser(L""));
    } catch (const winrt::hresult_error& e) { return Err(DracError(e)); }
  }
} // namespace package
  #endif // DRAC_ENABLE_PACKAGECOUNT

#endif // _WIN32
