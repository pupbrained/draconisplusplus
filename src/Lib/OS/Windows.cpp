#ifdef _WIN32

// clang-format off
#include <dxgi.h>
#include <dwmapi.h>
#include <algorithm>
#include <ranges>
#include <tlhelp32.h>
#include <wincrypt.h>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <intrin.h> 

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Management.Deployment.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.System.Profile.h>

#if DRAC_ENABLE_PACKAGECOUNT
  #include "Drac++/Services/PackageCounting.hpp"
#endif

#include "DracUtils/Env.hpp"

#include "Drac++/Core/System.hpp"
// clang-format on

using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

namespace {
  using util::error::DracError, util::error::DracErrorCode;
  using namespace util::types;

  constexpr const wchar_t* WINDOWS_10      = L"Windows 10";
  constexpr const wchar_t* WINDOWS_11      = L"Windows 11";
  constexpr const wchar_t* PRODUCT_NAME    = L"ProductName";
  constexpr const wchar_t* DISPLAY_VERSION = L"DisplayVersion";
  constexpr const wchar_t* CURRENT_BUILD   = L"CurrentBuildNumber";
  constexpr const wchar_t* SYSTEM_FAMILY   = L"SystemFamily";

  constexpr ULONG_PTR KUSER_SHARED_DATA           = 0x7FFE0000;                ///< Base address of the shared kernel-user data page
  constexpr ULONG     KUSER_SHARED_NtMajorVersion = KUSER_SHARED_DATA + 0x26C; ///< Offset to the NtMajorVersion field
  constexpr ULONG     KUSER_SHARED_NtMinorVersion = KUSER_SHARED_DATA + 0x270; ///< Offset to the NtMinorVersion field
  constexpr ULONG     KUSER_SHARED_NtBuildNumber  = KUSER_SHARED_DATA + 0x260; ///< Offset to the NtBuildNumber field

  [[nodiscard]] fn ConvertWStringToUTF8(const std::wstring& wstr) -> SZString {
    if (wstr.empty())
      return {};

    const int requiredSize = WideCharToMultiByte(
      CP_UTF8,
      0,
      wstr.c_str(),
      static_cast<int>(wstr.length()),
      nullptr,
      0,
      nullptr,
      nullptr
    );

    if (requiredSize == 0)
      return {};

    SZString result(requiredSize, '\0');

    const int bytesConverted = WideCharToMultiByte(
      CP_UTF8,
      0,
      wstr.c_str(),
      static_cast<int>(wstr.length()),
      result.data(),
      requiredSize,
      nullptr,
      nullptr
    );

    if (bytesConverted == 0)
      return {};

    return result;
  }

  const thread_local struct RegistryBuffer {
    mutable Array<wchar_t, 1024> data {};
  } registryBuffer;

  struct ProcessData {
    DWORD    parentPid = 0;
    SZString baseExeNameLower;
  };

  class RegistryCache {
   private:
    HKEY m_currentVersionKey = nullptr;
    HKEY m_hardwareConfigKey = nullptr;

    RegistryCache() {
      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", 0, KEY_READ, &m_currentVersionKey) != ERROR_SUCCESS)
        m_currentVersionKey = nullptr;
      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SYSTEM\HardwareConfig\Current)", 0, KEY_READ, &m_hardwareConfigKey) != ERROR_SUCCESS)
        m_hardwareConfigKey = nullptr;
    }

    ~RegistryCache() {
      if (m_currentVersionKey)
        RegCloseKey(m_currentVersionKey);
      if (m_hardwareConfigKey)
        RegCloseKey(m_hardwareConfigKey);
    }

   public:
    static fn getInstance() -> RegistryCache& {
      static RegistryCache Instance;
      return Instance;
    }

    fn getCurrentVersionKey() const -> HKEY {
      return m_currentVersionKey;
    }

    fn getHardwareConfigKey() const -> HKEY {
      return m_hardwareConfigKey;
    }

    RegistryCache(const RegistryCache&)                = delete;
    RegistryCache(RegistryCache&&)                     = delete;
    fn operator=(const RegistryCache&)->RegistryCache& = delete;
    fn operator=(RegistryCache&&)->RegistryCache&      = delete;
  };

  class ProcessTreeCache {
   private:
    std::unordered_map<DWORD, ProcessData> m_processMap;
    bool                                   m_initialized = false;

    ProcessTreeCache() = default;

   public:
    ~ProcessTreeCache() = default;

    static fn getInstance() -> ProcessTreeCache& {
      static ProcessTreeCache Instance;
      return Instance;
    }

    fn initialize() -> void {
      if (m_initialized)
        return;

      HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
      if (hSnap == INVALID_HANDLE_VALUE)
        return;

      PROCESSENTRY32W pe32;
      pe32.dwSize = sizeof(PROCESSENTRY32W);

      for (BOOL ok = Process32FirstW(hSnap, &pe32); ok; ok = Process32NextW(hSnap, &pe32)) {
        const wchar_t* const baseNamePtr = wcsrchr(pe32.szExeFile, L'\\');

        const std::wstring_view exeName = (baseNamePtr == nullptr)
          ? std::wstring_view(pe32.szExeFile)
          : std::wstring_view(baseNamePtr).substr(1);

        Array<wchar_t, MAX_PATH> tempBaseName {};
        wcsncpy_s(tempBaseName.data(), tempBaseName.size(), exeName.data(), exeName.size());

        _wcslwr_s(tempBaseName.data(), tempBaseName.size());

        std::wstring_view view(tempBaseName.data());

        if (view.ends_with(L".exe")) {
          std::span<wchar_t, MAX_PATH> bufferSpan(tempBaseName.data(), tempBaseName.size());
          bufferSpan[view.length() - 4] = L'\0';
        }

        const SZString finalBaseName = ConvertWStringToUTF8(tempBaseName.data());

        m_processMap[pe32.th32ProcessID] = ProcessData {
          .parentPid        = pe32.th32ParentProcessID,
          .baseExeNameLower = finalBaseName
        };
      }

      CloseHandle(hSnap);
      m_initialized = true;
    }

    [[nodiscard]] fn getProcessMap() const -> const std::unordered_map<DWORD, ProcessData>& {
      return m_processMap;
    }

    explicit ProcessTreeCache(std::unordered_map<DWORD, ProcessData> processMap) : m_processMap(std::move(processMap)) {}
    ProcessTreeCache(const ProcessTreeCache&)                = delete;
    ProcessTreeCache(ProcessTreeCache&&)                     = delete;
    fn operator=(const ProcessTreeCache&)->ProcessTreeCache& = delete;
    fn operator=(ProcessTreeCache&&)->ProcessTreeCache&      = delete;
  };

  // clang-format off
  constexpr Array<Pair<SZStringView, SZStringView>, 3> windowsShellMap = {{
    {        "cmd",  "Command Prompt" },
    { "powershell",      "PowerShell" },
    {       "pwsh", "PowerShell Core" },
  }};

  constexpr Array<Pair<SZStringView, SZStringView>, 3> msysShellMap = {{
    { "bash", "Bash" },
    {  "zsh",  "Zsh" },
    { "fish", "Fish" },
  }};
  // clang-format on

  fn GetDirCount(const std::wstring_view path) -> Result<u64> {
    std::wstring searchPath(path);
    searchPath.append(L"\\*");

    WIN32_FIND_DATAW findData;
    HANDLE           hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_NOT_FOUND)
        return 0;

      return Err(DracError(DracErrorCode::PlatformSpecific, "FindFirstFileW failed"));
    }

    u64 count = 0;

    while (hFind != INVALID_HANDLE_VALUE) {
      if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0))
        count++;

      if (!FindNextFileW(hFind, &findData))
        break;
    }

    FindClose(hFind);
    return count;
  }

  fn GetRegistryValue(const HKEY& hKey, const std::wstring& valueName) -> std::wstring {
    DWORD dataSizeInBytes = registryBuffer.data.size() * sizeof(wchar_t);
    DWORD type            = 0;

    if (RegQueryValueExW(
          hKey,
          valueName.c_str(),
          nullptr,
          &type,
          reinterpret_cast<LPBYTE>(registryBuffer.data.data()), // NOLINT(*-pro-type-reinterpret-cast)
          &dataSizeInBytes
        ) != ERROR_SUCCESS)
      return {};

    if (type == REG_SZ || type == REG_EXPAND_SZ)
      return registryBuffer.data.data();

    return {};
  }

  template <usize sz>
  fn FindShellInProcessTree(const DWORD startPid, const Array<Pair<SZStringView, SZStringView>, sz>& shellMap) -> Result<SZString> {
    if (startPid == 0)
      return Err(DracError(DracErrorCode::PlatformSpecific, "Start PID is 0"));

    ProcessTreeCache::getInstance().initialize();
    const std::unordered_map<DWORD, ProcessData>& processMap = ProcessTreeCache::getInstance().getProcessMap();

    DWORD         currentPid = startPid;
    constexpr int maxDepth   = 16;
    int           depth      = 0;

    while (currentPid != 0 && depth < maxDepth) {
      auto procIt = processMap.find(currentPid);

      if (procIt == processMap.end())
        break;

      const SZString& processName = procIt->second.baseExeNameLower;

      if (const auto mapIter = std::ranges::find_if(shellMap, [&](const Pair<SZStringView, SZStringView>& pair) { return SZStringView { processName } == pair.first; });
          mapIter != std::ranges::end(shellMap))
        return mapIter->second;

      currentPid = procIt->second.parentPid;
      depth++;
    }

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
    static MEMORYSTATUSEX MemInfo;
    MemInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&MemInfo))
      return ResourceUsage { .usedBytes = MemInfo.ullTotalPhys - MemInfo.ullAvailPhys, .totalBytes = MemInfo.ullTotalPhys };

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

  fn System::getOSVersion() -> Result<SZString> {
    try {
      RegistryCache& registry = RegistryCache::getInstance();

      HKEY currentVersionKey = registry.getCurrentVersionKey();

      if (!currentVersionKey)
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &currentVersionKey) != ERROR_SUCCESS)
          return Err(DracError(DracErrorCode::NotFound, "Failed to open registry key"));

      std::wstring productName = GetRegistryValue(currentVersionKey, PRODUCT_NAME);

      if (productName.empty())
        return Err(DracError(DracErrorCode::NotFound, "ProductName not found in registry"));

      if (const Result<u64> buildNumberOpt = GetBuildNumber()) {
        if (const u64 buildNumber = *buildNumberOpt; buildNumber >= 22000) {
          if (const size_t pos = productName.find(WINDOWS_10); pos != std::wstring::npos) {
            const bool startBoundary = (pos == 0 || !iswalnum(productName[pos - 1]));
            const bool endBoundary   = (pos + std::wcslen(WINDOWS_10) == productName.length() || !iswalnum(productName[pos + std::wcslen(WINDOWS_10)]));

            if (startBoundary && endBoundary)
              productName.replace(pos, std::wcslen(WINDOWS_10), WINDOWS_11);
          }
        }
      }

      const std::wstring displayVersion = GetRegistryValue(currentVersionKey, DISPLAY_VERSION);

      SZString productNameUTF8 = ConvertWStringToUTF8(productName);

      if (displayVersion.empty())
        return productNameUTF8;

      SZString displayVersionUTF8 = ConvertWStringToUTF8(displayVersion);

      return productNameUTF8 + " " + displayVersionUTF8;
    } catch (const std::exception& e) { return Err(DracError(e)); }
  }

  fn System::getHost() -> Result<SZString> {
    RegistryCache& registry          = RegistryCache::getInstance();
    HKEY           hardwareConfigKey = registry.getHardwareConfigKey();

    if (!hardwareConfigKey)
      if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\HardwareConfig\\Current", 0, KEY_READ, &hardwareConfigKey) != ERROR_SUCCESS)
        return Err(DracError(DracErrorCode::NotFound, "Failed to open registry key"));

    const std::wstring systemFamily = GetRegistryValue(hardwareConfigKey, SYSTEM_FAMILY);

    return ConvertWStringToUTF8(systemFamily);
  }

  fn System::getKernelVersion() -> Result<SZString> {
    // NOLINTBEGIN(*-pro-type-reinterpret-cast, *-no-int-to-ptr)
    const ULONG majorVersion = *reinterpret_cast<const ULONG*>(KUSER_SHARED_NtMajorVersion);
    const ULONG minorVersion = *reinterpret_cast<const ULONG*>(KUSER_SHARED_NtMinorVersion);
    const ULONG buildNumber  = *reinterpret_cast<const ULONG*>(KUSER_SHARED_NtBuildNumber);
    // NOLINTEND(*-pro-type-reinterpret-cast, *-no-int-to-ptr)

    return std::format("{}.{}.{}", majorVersion, minorVersion, buildNumber);
  }

  fn System::getWindowManager() -> Result<SZString> {
    BOOL compositionEnabled = FALSE;

    if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)))
      return compositionEnabled ? "DWM" : "Windows Manager (Basic)";

    return Err(DracError(DracErrorCode::NotFound, "Failed to get window manager (DwmIsCompositionEnabled failed"));
  }

  fn System::getDesktopEnvironment() -> Result<SZString> {
    RegistryCache& registry = RegistryCache::getInstance();

    HKEY currentVersionKey = registry.getCurrentVersionKey();

    if (!currentVersionKey)
      if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &currentVersionKey) != ERROR_SUCCESS)
        return Err(DracError(DracErrorCode::NotFound, "Failed to open registry key"));

    const std::wstring buildStr = GetRegistryValue(currentVersionKey, CURRENT_BUILD);

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
        const std::wstring productName = GetRegistryValue(currentVersionKey, PRODUCT_NAME);

        if (productName.find(L"Windows 10") != std::wstring::npos)
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

  fn System::getShell() -> Result<SZString> {
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
        SZString    shellExe  = (lastSlash != String::npos) ? shellPath.substr(lastSlash + 1) : shellPath;
        std::ranges::transform(shellExe, shellExe.begin(), [](const u8 character) { return std::tolower(character); });
        if (shellExe.ends_with(".exe"))
          shellExe.resize(shellExe.length() - 4);

        if (const auto iter = std::ranges::find_if(msysShellMap, [&](const Pair<SZStringView, SZStringView>& pair) { return SZStringView { shellExe } == pair.first; });
            iter != std::ranges::end(msysShellMap))
          return iter->second;

        return Err(DracError(DracErrorCode::NotFound, "Shell not found"));
      }

      const DWORD            currentPid = GetCurrentProcessId();
      const Result<SZString> msysShell  = FindShellInProcessTree(currentPid, msysShellMap);

      if (msysShell)
        return *msysShell;

      return Err(msysShell.error());
    }

    const DWORD currentPid = GetCurrentProcessId();

    if (const Result<SZString> windowsShell = FindShellInProcessTree(currentPid, windowsShellMap))
      return *windowsShell;

    return Err(DracError(DracErrorCode::NotFound, "Shell not found"));
  }

  fn System::getDiskUsage() -> Result<ResourceUsage> {
    ULARGE_INTEGER freeBytes, totalBytes;

    if (GetDiskFreeSpaceExW(L"C:\\\\", nullptr, &totalBytes, &freeBytes))
      return ResourceUsage { .usedBytes = totalBytes.QuadPart - freeBytes.QuadPart, .totalBytes = totalBytes.QuadPart };

    return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get disk usage"));
  }

  fn System::getCPUModel() -> Result<SZString> {
    Array<i32, 4>   cpuInfo     = { -1 };
    Array<char, 49> brandString = {};

    __cpuid(0x80000000, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);

    const u32 maxFunction = cpuInfo[0];

    if (maxFunction < 0x80000004)
      return Err(DracError(DracErrorCode::PlatformSpecific, "CPU does not support brand string"));

    for (u32 i = 0; i < 3; i++) {
      __cpuid(0x80000002 + i, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
      std::memcpy(&brandString.at(i * 16), cpuInfo.data(), sizeof(cpuInfo));
    }

    SZString result(brandString.data());

    while (!result.empty() && std::isspace(result.back()))
      result.pop_back();

    if (result.empty())
      return Err(DracError(DracErrorCode::NotFound, "Failed to get CPU model"));

    return result;
  }

  fn System::getGPUModel() -> Result<SZString> {
    IDXGIFactory* pFactory = nullptr;

  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wlanguage-extension-token"
    // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pFactory))))
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to create DXGI Factory"));
  #pragma clang diagnostic pop

    IDXGIAdapter* pAdapter = nullptr;

    if (pFactory->EnumAdapters(0, &pAdapter) == DXGI_ERROR_NOT_FOUND) {
      pFactory->Release();
      return Err(DracError(DracErrorCode::NotFound, "No DXGI adapters found"));
    }

    DXGI_ADAPTER_DESC desc {};

    if (FAILED(pAdapter->GetDesc(&desc))) {
      pAdapter->Release();
      pFactory->Release();
      return Err(DracError(DracErrorCode::PlatformSpecific, "Failed to get adapter description"));
    }

    Array<char, 128> gpuName {};

    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpuName.data(), gpuName.size(), nullptr, nullptr);

    pAdapter->Release();
    pFactory->Release();

    return gpuName.data();
  }
} // namespace os

  #if DRAC_ENABLE_PACKAGECOUNT
namespace package {
  using util::helpers::GetEnvW;

  fn CountChocolatey() -> Result<u64> {
    std::wstring chocoPath = L"C:\\ProgramData\\chocolatey";

    if (auto chocoEnv = GetEnvW(L"ChocolateyInstall"); chocoEnv)
      chocoPath = *chocoEnv;

    chocoPath.append(L"\\lib");
    return GetDirCount(chocoPath);
  }

  fn CountScoop() -> Result<u64> {
    std::wstring scoopAppsPath;

    if (auto scoopEnv = GetEnvW(L"SCOOP"); scoopEnv) {
      scoopAppsPath = *scoopEnv;
      scoopAppsPath.append(L"\\apps");
    } else if (auto userProfile = GetEnvW(L"USERPROFILE"); userProfile) {
      scoopAppsPath = *userProfile;
      scoopAppsPath.append(L"\\scoop\\apps");
    } else
      return Err(DracError(DracErrorCode::NotFound, "Could not determine Scoop installation directory (SCOOP and USERPROFILE environment variables not found)"));

    return GetDirCount(scoopAppsPath);
  }

  fn CountWinGet() -> Result<u64> {
    try {
      return std::ranges::distance(winrt::Windows::Management::Deployment::PackageManager().FindPackagesForUser(L""));
    } catch (const winrt::hresult_error& e) { return Err(DracError(e)); }
  }
} // namespace package
  #endif // DRAC_ENABLE_PACKAGECOUNT

#endif // _WIN32
