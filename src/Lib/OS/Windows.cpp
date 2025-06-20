/**
 * @file      Windows.cpp
 * @author    pupbrained (mars@pupbrained.dev)
 * @brief     Provides the Windows-specific implementation of the System class for system information retrieval.
 *
 * @details This file contains the concrete implementation of the System class interface for the
 * Microsoft Windows platform. It retrieves a wide range of system information by
 * leveraging various Windows APIs, including:
 * - Standard Win32 API for memory, disk, and process information.
 * - Windows Registry for OS version, host model, and CPU details.
 * - DirectX Graphics Infrastructure (DXGI) for enumerating graphics adapters.
 * - Windows Runtime (WinRT) for modern OS details, media controls, and WinGet packages.
 *
 * To optimize performance, the implementation caches process snapshots and registry
 * handles.
 *
 * @see draconis::core::system::System
 */

#ifdef _WIN32

  #include <dwmapi.h>                               // DwmIsCompositionEnabled
  #include <dxgi.h>                                 // IDXGIFactory, IDXGIAdapter, DXGI_ADAPTER_DESC
  #include <tlhelp32.h>                             // CreateToolhelp32Snapshot, PROCESSENTRY32W, Process32FirstW, Process32NextW, TH32CS_SNAPPROCESS
  #include <winrt/Windows.Foundation.Collections.h> // winrt::Windows::Foundation::Collections::Map
  #include <winrt/Windows.Management.Deployment.h>  // winrt::Windows::Management::Deployment::PackageManager
  #include <winrt/Windows.Media.Control.h>          // winrt::Windows::Media::Control::MediaProperties
  #include <winrt/Windows.System.Profile.h>         // winrt::Windows::System::Profile::AnalyticsInfo

  #if DRAC_ENABLE_PACKAGECOUNT
    #include "Drac++/Services/Packages.hpp"
  #endif

  #include "Drac++/Core/System.hpp"

  #include "DracUtils/Env.hpp"

  #include "Utils/Caching.hpp"

namespace {
  using draconis::utils::error::DracError;
  using enum draconis::utils::error::DracErrorCode;
  using namespace draconis::utils::types;

  // Display names for Windows 10 and 11
  constexpr const wchar_t* WINDOWS_10 = L"Windows 10";
  constexpr const wchar_t* WINDOWS_11 = L"Windows 11";

  // Registry keys for Windows version information
  constexpr const wchar_t* PRODUCT_NAME    = L"ProductName";
  constexpr const wchar_t* DISPLAY_VERSION = L"DisplayVersion";
  constexpr const wchar_t* SYSTEM_FAMILY   = L"SystemFamily";

  // clang-format off
  // Shell map for Windows
  constexpr Array<Pair<StringView, StringView>, 5> windowsShellMap = {{
    {        "cmd",   "Command Prompt" },
    { "powershell",       "PowerShell" },
    {       "pwsh",  "PowerShell Core" },
    {         "wt", "Windows Terminal" },
    {   "explorer", "Windows Explorer" },
  }};

  // Shell map for MSYS2
  constexpr Array<Pair<StringView, StringView>, 7> msysShellMap = {{
    { "bash",      "Bash" },
    {  "zsh",       "Zsh" },
    { "fish",      "Fish" },
    {   "sh",        "sh" },
    {  "ksh", "KornShell" },
    { "tcsh",      "tcsh" },
    { "dash",      "dash" },
  }};
  // clang-format on

  // Convert a wide string to a UTF-8 string
  [[nodiscard]] fn ConvertWStringToUTF8(const std::wstring& wstr) -> String {
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

    String result(requiredSize, '\0');

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

  /**
   * @brief A thread-local buffer for Windows Registry API calls.
   * @details This avoids repeated heap allocations within functions that query the
   * registry, such as `GetRegistryValue`. It is defined as `thread_local` to ensure
   * thread safety without requiring mutexes.
   */
  constexpr thread_local struct RegistryBuffer {
    mutable Array<wchar_t, 1024> data {};
  } registryBuffer;

  /**
   * @brief Holds essential data for a single process in the process tree.
   * @details Used by the `ProcessTreeCache` to store a simplified view of each
   * process, containing only the information needed for shell detection.
   */
  struct ProcessData {
    DWORD  parentPid = 0;    ///< The process ID of the parent process.
    String baseExeNameLower; ///< The lowercase executable name without path or extension.
  };

  /**
   * @brief A singleton cache for frequently used Windows Registry keys.
   * @details This class opens handles to common registry keys upon first use and
   * keeps them open for the lifetime of the application. This avoids the overhead
   * of repeatedly calling `RegOpenKeyEx` and `RegCloseKey`. The destructor ensures
   * that the handles are properly closed on exit (RAII).
   */
  class RegistryCache {
   private:
    HKEY m_currentVersionKey = nullptr; ///< The handle to the current version key.
    HKEY m_hardwareConfigKey = nullptr; ///< The handle to the hardware config key.

    /**
     * @brief Opens the current version and hardware config keys.
     * @details This constructor opens the current version and hardware config keys,
     * and stores the handles in the class.
     */
    RegistryCache() {
      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", 0, KEY_READ, &m_currentVersionKey) != ERROR_SUCCESS)
        m_currentVersionKey = nullptr;
      if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, R"(SYSTEM\HardwareConfig\Current)", 0, KEY_READ, &m_hardwareConfigKey) != ERROR_SUCCESS)
        m_hardwareConfigKey = nullptr;
    }

    /**
     * @brief Closes the current version and hardware config keys.
     * @details This destructor closes the current version and hardware config keys,
     * and stores the handles in the class.
     */
    ~RegistryCache() {
      if (m_currentVersionKey)
        RegCloseKey(m_currentVersionKey);
      if (m_hardwareConfigKey)
        RegCloseKey(m_hardwareConfigKey);
    }

   public:
    /**
     * @brief Gets the instance of the RegistryCache class.
     * @details This function returns the instance of the RegistryCache class.
     */
    static fn getInstance() -> RegistryCache& {
      static RegistryCache Instance;
      return Instance;
    }

    /**
     * @brief Gets the handle to the current version key.
     * @details This function returns the handle to the current version key.
     */
    fn getCurrentVersionKey() const -> HKEY {
      return m_currentVersionKey;
    }

    /**
     * @brief Gets the handle to the hardware config key.
     * @details This function returns the handle to the hardware config key.
     */
    fn getHardwareConfigKey() const -> HKEY {
      return m_hardwareConfigKey;
    }

    RegistryCache(const RegistryCache&)                = delete;
    RegistryCache(RegistryCache&&)                     = delete;
    fn operator=(const RegistryCache&)->RegistryCache& = delete;
    fn operator=(RegistryCache&&)->RegistryCache&      = delete;
  };

  /**
   * @brief A singleton cache for a snapshot of the system's process tree.
   * @details This class creates a complete snapshot of all running processes on
   * its first use (`initialize()`). It stores a simplified map of process data,
   * allowing for efficient and repeated lookups of parent processes without needing
   * to re-query the OS each time. This is primarily used for finding the user's shell.
   */
  class ProcessTreeCache {
   private:
    std::unordered_map<DWORD, ProcessData> m_processMap;          ///< The map of processes.
    bool                                   m_initialized = false; ///< Whether the process tree cache has been initialized.

    ProcessTreeCache() = default;

   public:
    ~ProcessTreeCache() = default;

    /**
     * @brief Gets the instance of the ProcessTreeCache class.
     * @details This function returns the instance of the ProcessTreeCache class.
     */
    static fn getInstance() -> ProcessTreeCache& {
      static ProcessTreeCache Instance;
      return Instance;
    }

    /**
     * @brief Initializes the process tree cache.
     * @details This function initializes the process tree cache,
     * and stores the handles in the class.
     */
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

        if (std::wstring_view view(tempBaseName.data()); view.ends_with(L".exe")) {
          Span<wchar_t, MAX_PATH> bufferSpan(tempBaseName.data(), tempBaseName.size());
          bufferSpan[view.length() - 4] = L'\0';
        }

        const String finalBaseName = ConvertWStringToUTF8(tempBaseName.data());

        m_processMap[pe32.th32ProcessID] = ProcessData {
          .parentPid        = pe32.th32ParentProcessID,
          .baseExeNameLower = finalBaseName
        };
      }

      CloseHandle(hSnap);
      m_initialized = true;
    }

    /**
     * @brief Gets the process map.
     * @details This function returns the process map.
     */
    [[nodiscard]] fn getProcessMap() const -> const std::unordered_map<DWORD, ProcessData>& {
      return m_processMap;
    }

    ProcessTreeCache(const ProcessTreeCache&)                = delete;
    ProcessTreeCache(ProcessTreeCache&&)                     = delete;
    fn operator=(const ProcessTreeCache&)->ProcessTreeCache& = delete;
    fn operator=(ProcessTreeCache&&)->ProcessTreeCache&      = delete;
  };

  fn GetDirCount(const std::wstring_view path) -> Result<u64> {
    std::wstring searchPath(path);
    searchPath.append(L"\\*");

    WIN32_FIND_DATAW findData;
    HANDLE           hFind = FindFirstFileW(searchPath.c_str(), &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
      if (GetLastError() == ERROR_FILE_NOT_FOUND)
        return 0;

      return Err(DracError(PlatformSpecific, "FindFirstFileW failed"));
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
  fn FindShellInProcessTree(const DWORD startPid, const Array<Pair<StringView, StringView>, sz>& shellMap) -> Result<String> {
    if (startPid == 0)
      return Err(DracError(PlatformSpecific, "Start PID is 0"));

    ProcessTreeCache::getInstance().initialize();
    const std::unordered_map<DWORD, ProcessData>& processMap = ProcessTreeCache::getInstance().getProcessMap();

    DWORD         currentPid = startPid;
    constexpr int maxDepth   = 16;
    int           depth      = 0;

    while (currentPid != 0 && depth < maxDepth) {
      auto procIt = processMap.find(currentPid);

      if (procIt == processMap.end())
        break;

      const String& processName = procIt->second.baseExeNameLower;

      if (const auto mapIter = std::ranges::find_if(shellMap, [&](const Pair<StringView, StringView>& pair) { return StringView { processName } == pair.first; });
          mapIter != std::ranges::end(shellMap))
        return String(mapIter->second);

      currentPid = procIt->second.parentPid;
      depth++;
    }

    return Err(DracError(NotFound, "Shell not found"));
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

    return Err(DracError(NotFound, "Failed to get build number"));
  }
} // namespace

namespace draconis::core::system {
  fn System::getMemInfo() -> Result<ResourceUsage> {
    static MEMORYSTATUSEX MemInfo;
    MemInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&MemInfo))
      return ResourceUsage { .usedBytes = MemInfo.ullTotalPhys - MemInfo.ullAvailPhys, .totalBytes = MemInfo.ullTotalPhys };

    return Err(DracError(PlatformSpecific, std::format("GlobalMemoryStatusEx failed with error code {}", GetLastError())));
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

      return Err(DracError(NotFound, "No media session found"));
    } catch (const winrt::hresult_error& e) { return Err(DracError(e)); }
  }
  #endif // DRAC_ENABLE_NOWPLAYING

  fn System::getOSVersion() -> Result<String> {
    const RegistryCache& registry = RegistryCache::getInstance();

    HKEY currentVersionKey = registry.getCurrentVersionKey();

    if (!currentVersionKey)
      return Err(DracError(NotFound, "Failed to open registry key"));

    std::wstring productName = GetRegistryValue(currentVersionKey, PRODUCT_NAME);

    if (productName.empty())
      return Err(DracError(NotFound, "ProductName not found in registry"));

    if (const Result<u64> buildNumberOpt = GetBuildNumber())
      if (const u64 buildNumber = *buildNumberOpt; buildNumber >= 22000)
        if (const size_t pos = productName.find(WINDOWS_10); pos != std::wstring::npos) {
          const bool startBoundary = (pos == 0 || !iswalnum(productName[pos - 1]));
          // ReSharper disable once CppTooWideScopeInitStatement
          const bool endBoundary = (pos + std::wcslen(WINDOWS_10) == productName.length() || !iswalnum(productName[pos + std::wcslen(WINDOWS_10)]));

          if (startBoundary && endBoundary)
            productName.replace(pos, std::wcslen(WINDOWS_10), WINDOWS_11);
        }

    const std::wstring displayVersion = GetRegistryValue(currentVersionKey, DISPLAY_VERSION);

    String productNameUTF8 = ConvertWStringToUTF8(productName);

    if (displayVersion.empty())
      return productNameUTF8;

    const String displayVersionUTF8 = ConvertWStringToUTF8(displayVersion);

    return productNameUTF8 + " " + displayVersionUTF8;
  }

  fn System::getHost() -> Result<String> {
    const RegistryCache& registry          = RegistryCache::getInstance();
    HKEY                 hardwareConfigKey = registry.getHardwareConfigKey();

    if (!hardwareConfigKey)
      return Err(DracError(NotFound, "Failed to open registry key"));

    const std::wstring systemFamily = GetRegistryValue(hardwareConfigKey, SYSTEM_FAMILY);

    return ConvertWStringToUTF8(systemFamily);
  }

  fn System::getKernelVersion() -> Result<String> {
    fn filter = [](const u32 code) -> i32 {
      if (code == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_EXECUTE_HANDLER;

      return EXCEPTION_CONTINUE_SEARCH;
    };

    constexpr ULONG_PTR kuserSharedData = 0x7FFE0000; ///< Base address of the shared kernel-user data page

    constexpr u32 kuserSharedNtMajorVersion = kuserSharedData + 0x26C; ///< Offset to the NtMajorVersion field
    constexpr u32 kuserSharedNtMinorVersion = kuserSharedData + 0x270; ///< Offset to the NtMinorVersion field
    constexpr u32 kuserSharedNtBuildNumber  = kuserSharedData + 0x260; ///< Offset to the NtBuildNumber field

    u32 majorVersion = 0;
    u32 minorVersion = 0;
    u32 buildNumber  = 0;

  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wlanguage-extension-token"
    // NOLINTBEGIN(*-pro-type-reinterpret-cast, *-no-int-to-ptr)
    __try {
      majorVersion = *reinterpret_cast<const volatile u32*>(kuserSharedNtMajorVersion);
      minorVersion = *reinterpret_cast<const volatile u32*>(kuserSharedNtMinorVersion);
      buildNumber  = *reinterpret_cast<const volatile u32*>(kuserSharedNtBuildNumber);
    } __except (filter(GetExceptionCode())) {
      return Err(DracError(PlatformSpecific, "Failed to read kernel version from KUSER_SHARED_DATA"));
    }
    // NOLINTEND(*-pro-type-reinterpret-cast, *-no-int-to-ptr)
  #pragma clang diagnostic pop

    return std::format("{}.{}.{}", majorVersion, minorVersion, buildNumber);
  }

  fn System::getWindowManager() -> Result<String> {
    BOOL compositionEnabled = FALSE;

    if (SUCCEEDED(DwmIsCompositionEnabled(&compositionEnabled)))
      return compositionEnabled ? "DWM" : "Windows Manager (Basic)";

    return Err(DracError(NotFound, "Failed to get window manager (DwmIsCompositionEnabled failed"));
  }

  fn System::getDesktopEnvironment() -> Result<String> {
    const Result<u64> buildResult = GetBuildNumber();
    if (!buildResult)
      return Err(buildResult.error());

    const u64 build = *buildResult;

    // Windows 11+ (Fluent)
    if (build >= 22000)
      return "Fluent (Windows 11)";

    // Windows 10 Fluent Era
    if (build >= 15063)
      return "Fluent (Windows 10)";

    // Windows 8.1/10 Metro Era
    if (build >= 9200) { // Windows 8+
      // Distinguish between Windows 8 and 10
      if (GetRegistryValue(RegistryCache::getInstance().getCurrentVersionKey(), PRODUCT_NAME).find(L"Windows 10") != std::wstring::npos)
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
  }

  fn System::getShell() -> Result<String> {
    using draconis::utils::env::GetEnv;

    if (const Result<String> msystemResult = GetEnv("MSYSTEM"); msystemResult && !msystemResult->empty()) {
      String shellPath;

      if (const Result<String> shellResult = GetEnv("SHELL"); shellResult && !shellResult->empty())
        shellPath = *shellResult;
      else if (const Result<String> loginShellResult = GetEnv("LOGINSHELL"); loginShellResult && !loginShellResult->empty())
        shellPath = *loginShellResult;

      if (!shellPath.empty()) {
        const usize lastSlash = shellPath.find_last_of("\\/");
        String      shellExe  = (lastSlash != String::npos) ? shellPath.substr(lastSlash + 1) : shellPath;
        std::ranges::transform(shellExe, shellExe.begin(), [](const u8 character) { return std::tolower(character); });
        if (shellExe.ends_with(".exe"))
          shellExe.resize(shellExe.length() - 4);

        if (const auto iter = std::ranges::find_if(msysShellMap, [&](const Pair<StringView, StringView>& pair) { return StringView { shellExe } == pair.first; }); iter != std::ranges::end(msysShellMap))
          return String(iter->second);

        return Err(DracError(NotFound, "Shell not found"));
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

    return Err(DracError(NotFound, "Shell not found"));
  }

  fn System::getDiskUsage() -> Result<ResourceUsage> {
    ULARGE_INTEGER freeBytes, totalBytes;

    if (FAILED(GetDiskFreeSpaceExW(L"C:\\\\", nullptr, &totalBytes, &freeBytes)))
      return Err(DracError(PlatformSpecific, "Failed to get disk usage"));

    return ResourceUsage { .usedBytes = totalBytes.QuadPart - freeBytes.QuadPart, .totalBytes = totalBytes.QuadPart };
  }

  fn System::getCPUModel() -> Result<String> {
  #if DRAC_ARCH_X86_64 || DRAC_ARCH_I686
    Array<i32, 4>   cpuInfo     = { -1 };
    Array<char, 49> brandString = {};

    __cpuid(0x80000000, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);

    if (const u32 maxFunction = cpuInfo[0]; maxFunction < 0x80000004)
      return Err(DracError(PlatformSpecific, "CPU does not support brand string"));

    for (u32 i = 0; i < 3; i++) {
      __cpuid(0x80000002 + i, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);
      std::memcpy(&brandString.at(i * 16), cpuInfo.data(), sizeof(cpuInfo));
    }

    String result(brandString.data());

    while (!result.empty() && std::isspace(result.back()))
      result.pop_back();

    if (result.empty())
      return Err(DracError(NotFound, "Failed to get CPU model"));

    return result;
  #else
    HKEY hKey = nullptr;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
      Array<wchar_t, 256> szBuffer = {};

      // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
      if (RegQueryValueExW(hKey, L"ProcessorNameString", nullptr, nullptr, reinterpret_cast<LPBYTE>(szBuffer.data()), &szBuffer.size()) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return ConvertWStringToUTF8(szBuffer);
      }

      RegCloseKey(hKey);
    }

    return Err(DracError(NotFound, "Failed to get CPU model from registry"));
  #endif
  }

  fn System::getGPUModel() -> Result<String> {
    IDXGIFactory* pFactory = nullptr;

  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wlanguage-extension-token"
    // NOLINTNEXTLINE(*-pro-type-reinterpret-cast)
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pFactory))))
      return Err(DracError(PlatformSpecific, "Failed to create DXGI Factory"));
  #pragma clang diagnostic pop

    IDXGIAdapter* pAdapter = nullptr;

    if (pFactory->EnumAdapters(0, &pAdapter) == DXGI_ERROR_NOT_FOUND) {
      pFactory->Release();
      return Err(DracError(NotFound, "No DXGI adapters found"));
    }

    DXGI_ADAPTER_DESC desc {};

    if (FAILED(pAdapter->GetDesc(&desc))) {
      pAdapter->Release();
      pFactory->Release();
      return Err(DracError(PlatformSpecific, "Failed to get adapter description"));
    }

    Array<char, 128> gpuName {};

    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpuName.data(), gpuName.size(), nullptr, nullptr);

    pAdapter->Release();
    pFactory->Release();

    return gpuName.data();
  }
} // namespace draconis::core::system

  #if DRAC_ENABLE_PACKAGECOUNT
namespace draconis::services::packages {
  using draconis::utils::cache::GetValidCache, draconis::utils::cache::WriteCache;
  using draconis::utils::env::GetEnvW;

  fn CountChocolatey() -> Result<u64> {
    const String cacheKey = "chocolatey_";

    if (Result<u64> cachedCount = GetValidCache<u64>(cacheKey))
      return *cachedCount;
    else
      debug_at(cachedCount.error());

    std::wstring chocoPath = L"C:\\ProgramData\\chocolatey";

    if (auto chocoEnv = GetEnvW(L"ChocolateyInstall"); chocoEnv)
      chocoPath = *chocoEnv;

    chocoPath.append(L"\\lib");

    if (Result<u64> dirCount = GetDirCount(chocoPath)) {
      if (Result writeResult = WriteCache(cacheKey, *dirCount); !writeResult)
        debug_at(writeResult.error());

      return *dirCount;
    }

    return Err(DracError(NotFound, "Failed to get Chocolatey package count"));
  }

  fn CountScoop() -> Result<u64> {
    const String cacheKey = "scoop_";

    if (Result<u64> cachedCount = GetValidCache<u64>(cacheKey))
      return *cachedCount;
    else
      debug_at(cachedCount.error());

    std::wstring scoopAppsPath;

    if (auto scoopEnv = GetEnvW(L"SCOOP"); scoopEnv) {
      scoopAppsPath = *scoopEnv;
      scoopAppsPath.append(L"\\apps");
    } else if (auto userProfile = GetEnvW(L"USERPROFILE"); userProfile) {
      scoopAppsPath = *userProfile;
      scoopAppsPath.append(L"\\scoop\\apps");
    } else
      return Err(DracError(NotFound, "Could not determine Scoop installation directory (SCOOP and USERPROFILE environment variables not found)"));

    if (Result<u64> dirCount = GetDirCount(scoopAppsPath)) {
      if (Result writeResult = WriteCache(cacheKey, *dirCount); !writeResult)
        debug_at(writeResult.error());

      return *dirCount;
    }

    return Err(DracError(NotFound, "Failed to get Scoop package count"));
  }

  fn CountWinGet() -> Result<u64> {
    const String cacheKey = "winget_";

    if (Result<u64> cachedCount = GetValidCache<u64>(cacheKey))
      return *cachedCount;
    else
      debug_at(cachedCount.error());

    try {
      const u64 count = std::ranges::distance(winrt::Windows::Management::Deployment::PackageManager().FindPackagesForUser(L""));

      if (Result writeResult = WriteCache(cacheKey, count); !writeResult)
        debug_at(writeResult.error());

      return count;
    } catch (const winrt::hresult_error& e) { return Err(DracError(e)); }
  }
} // namespace draconis::services::packages
  #endif // DRAC_ENABLE_PACKAGECOUNT

#endif // _WIN32
