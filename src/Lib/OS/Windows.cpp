/**
 * @file   Windows.cpp
 * @author pupbrained (mars@pupbrained.dev)
 * @brief  Provides the Windows-specific implementation of the System class for system information retrieval.
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
 * handles. Wide strings are used for all string operations to avoid the overhead of
 * converting between UTF-8 and UTF-16 until the final result is needed.
 *
 * @see draconis::core::system
 */

#ifdef _WIN32

  #include <dxgi.h>                                 // IDXGIFactory, IDXGIAdapter, DXGI_ADAPTER_DESC
  #include <ranges>                                 // std::ranges::find_if, std::ranges::views::transform
  #include <tlhelp32.h>                             // CreateToolhelp32Snapshot, PROCESSENTRY32W, Process32FirstW, Process32NextW, TH32CS_SNAPPROCESS
  #include <winerror.h>                             // DXGI_ERROR_NOT_FOUND, ERROR_FILE_NOT_FOUND, ERROR_SUCCESS, FAILED, SUCCEEDED
  #include <winrt/Windows.Foundation.Collections.h> // winrt::Windows::Foundation::Collections::Map
  #include <winrt/Windows.Management.Deployment.h>  // winrt::Windows::Management::Deployment::PackageManager
  #include <winrt/Windows.Media.Control.h>          // winrt::Windows::Media::Control::MediaProperties
  #include <winrt/Windows.System.Profile.h>         // winrt::Windows::System::Profile::AnalyticsInfo

  #if DRAC_ENABLE_PACKAGECOUNT
    #include "Drac++/Services/Packages.hpp"
  #endif

  #include "Drac++/Core/System.hpp"

  #include "Drac++/Utils/Env.hpp"
  #include "Drac++/Utils/Error.hpp"
  #include "Drac++/Utils/Types.hpp"

namespace {
  using draconis::utils::error::DracError;
  using enum draconis::utils::error::DracErrorCode;
  using namespace draconis::utils::types;

  namespace constants {
    // Registry keys for Windows version information
    constexpr const wchar_t* PRODUCT_NAME    = L"ProductName";
    constexpr const wchar_t* DISPLAY_VERSION = L"DisplayVersion";
    constexpr const wchar_t* SYSTEM_FAMILY   = L"SystemFamily";

    // clang-format off
    constexpr Array<Pair<StringView, StringView>, 5> windowsShellMap = {{
      {      "cmd",     "Command Prompt" },
      { "powershell",       "PowerShell" },
      {       "pwsh",  "PowerShell Core" },
      {         "wt", "Windows Terminal" },
      {   "explorer", "Windows Explorer" },
    }};

    constexpr Array<Pair<StringView, StringView>, 7> msysShellMap = {{
      { "bash",      "Bash" },
      {  "zsh",       "Zsh" },
      { "fish",      "Fish" },
      {   "sh",        "sh" },
      {  "ksh", "KornShell" },
      { "tcsh",      "tcsh" },
      { "dash",      "dash" },
    }};

    constexpr Array<Pair<StringView, StringView>, 4> windowManagerMap = {{
      {     "glazewm",   "GlazeWM" },
      {    "komorebi",  "Komorebi" },
      {   "seelen-ui", "Seelen UI" },
      { "slu-service", "Seelen UI" },
    }};
    // clang-format on

  } // namespace constants

  namespace helpers {
    fn ConvertWStringToUTF8(const std::wstring& wstr) -> Result<String> {
      // Likely best to just return an empty string if an empty wide string is provided.
      if (wstr.empty())
        return String {};

      // First call WideCharToMultiByte to get the buffer size...
      const i32 sizeNeeded = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        nullptr,
        0,
        nullptr,
        nullptr
      );

      if (sizeNeeded == 0)
        return Err(DracError(PlatformSpecific, std::format("Failed to get buffer size for UTF-8 conversion. Error code: {}", GetLastError())));

      // Then make the buffer using that size...
      String result(sizeNeeded, 0);

      // ...and finally call WideCharToMultiByte again to convert the wide string to UTF-8.
      const i32 bytesConverted = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        result.data(),
        sizeNeeded,
        nullptr,
        nullptr
      );

      if (bytesConverted == 0)
        return Err(DracError(PlatformSpecific, std::format("Failed to convert wide string to UTF-8. Error code: {}", GetLastError())));

      return result;
    }

    fn GetDirCount(const std::wstring_view path) -> Result<u64> {
      // Create mutable copy and append wildcard.
      std::wstring searchPath(path);
      searchPath.append(L"\\*");

      // Used to receive information about the found file or directory.
      WIN32_FIND_DATAW findData;

      // Begin searching for files and directories.
      HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);

      if (hFind == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
          return 0;

        return Err(DracError(PlatformSpecific, "FindFirstFileW failed"));
      }

      u64 count = 0;

      while (hFind != INVALID_HANDLE_VALUE) {
        // Only increment if the found item is:
        // 1. a directory
        // 2. not a special directory (".", "..")
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && (wcscmp(findData.cFileName, L".") != 0 && wcscmp(findData.cFileName, L"..") != 0))
          count++;

        // Continue searching for more files and directories.
        if (!FindNextFileW(hFind, &findData))
          break;
      }

      // Ensure that the handle is closed to avoid leaks.
      FindClose(hFind);

      return count;
    }

    // Reads a registry value into a wstring.
    fn GetRegistryValue(const HKEY& hKey, const std::wstring& valueName) -> Result<std::wstring> {
      // Buffer for storing the registry value. Should be large enough to hold most values.
      Array<wchar_t, 1024> registryBuffer {};

      // Size of the buffer in bytes.
      DWORD dataSizeInBytes = registryBuffer.size() * sizeof(wchar_t);

      // Stores the type of the registry value.
      DWORD type = 0;

      // Query the registry value.
      if (
        const LSTATUS status = RegQueryValueExW(
          hKey,
          valueName.c_str(),
          nullptr,
          &type,
          reinterpret_cast<LPBYTE>(registryBuffer.data()), // NOLINT(*-pro-type-reinterpret-cast) - reinterpret_cast is required to convert the buffer to a byte array.
          &dataSizeInBytes
        );
        status != ERROR_SUCCESS
      ) {
        if (status == ERROR_FILE_NOT_FOUND)
          return Err(DracError(NotFound, "Registry value not found"));

        return Err(DracError(PlatformSpecific, "RegQueryValueExW failed with error code: " + std::to_string(status)));
      }

      // Ensure the retrieved value is a string.
      if (type == REG_SZ || type == REG_EXPAND_SZ)
        return std::wstring(registryBuffer.data());

      return Err(DracError(ParseError, "Registry value exists but is not a string type. Type is: " + std::to_string(type)));
    }

  } // namespace helpers

  namespace cache {
    // Caches registry values, allowing them to only be retrieved once.
    class RegistryCache {
     private:
      HKEY m_currentVersionKey = nullptr;
      HKEY m_hardwareConfigKey = nullptr;

      RegistryCache() {
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, LR"(SOFTWARE\Microsoft\Windows NT\CurrentVersion)", 0, KEY_READ, &m_currentVersionKey) != ERROR_SUCCESS)
          m_currentVersionKey = nullptr;

        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, LR"(SYSTEM\HardwareConfig\Current)", 0, KEY_READ, &m_hardwareConfigKey) != ERROR_SUCCESS)
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

    // Caches OS version data for use in other functions.
    class OsVersionCache {
     public:
      struct VersionData {
        u32 majorVersion;
        u32 minorVersion;
        u32 buildNumber;
      };

      static fn getInstance() -> const OsVersionCache& {
        static OsVersionCache Instance;
        return Instance;
      }

      fn getVersionData() const -> const Result<VersionData>& {
        return m_versionData;
      }

      fn getBuildNumber() const -> Result<u64> {
        if (!m_versionData)
          return Err(m_versionData.error());

        return static_cast<u64>(m_versionData->buildNumber);
      }

      OsVersionCache(const OsVersionCache&)                = delete;
      OsVersionCache(OsVersionCache&&)                     = delete;
      fn operator=(const OsVersionCache&)->OsVersionCache& = delete;
      fn operator=(OsVersionCache&&)->OsVersionCache&      = delete;

     private:
      Result<VersionData> m_versionData;

      // Fetching version data from KUSER_SHARED_DATA is the fastest way to get the version information.
      // It also avoids the need for a system call or registry access. The biggest downside, though, is
      // that it's inherently risky/unsafe, and could break in future updates. To mitigate this risk,
      // this constructor uses SEH (__try/__except) to handle potential exceptions and safely error out.
      OsVersionCache() {
        // KUSER_SHARED_DATA is a block of memory shared between the kernel and user-mode
        // processes. This address has not changed since its inception. It SHOULD always
        // contain data for the running Windows version.
        constexpr ULONG_PTR kuserSharedData = 0x7FFE0000;

        // These offsets should also be static/consistent across different versions of Windows.
        constexpr u32 kuserSharedNtMajorVersion = kuserSharedData + 0x26C;
        constexpr u32 kuserSharedNtMinorVersion = kuserSharedData + 0x270;
        constexpr u32 kuserSharedNtBuildNumber  = kuserSharedData + 0x260;

        // Considering this file is windows-specific, it's fine to use windows-specific extensions.
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wlanguage-extension-token"
        // Use Structured Exception Handling (SEH) to safely read the version data. In case of invalid
        // pointers, this will catch the access violation and return an Error, instead of crashing.
        __try {
          // Read the version data directly from the calculated memory addresses.
          // - reinterpret_cast is required to cast the memory addresses to volatile pointers.
          // - `volatile` tells the compiler that these memory reads should not be optimized away.
          m_versionData = VersionData {
            // NOLINTBEGIN(*-pro-type-reinterpret-cast, *-no-int-to-ptr)
            .majorVersion = *reinterpret_cast<const volatile u32*>(kuserSharedNtMajorVersion),
            .minorVersion = *reinterpret_cast<const volatile u32*>(kuserSharedNtMinorVersion),
            .buildNumber  = *reinterpret_cast<const volatile u32*>(kuserSharedNtBuildNumber)
            // NOLINTEND(*-pro-type-reinterpret-cast, *-no-int-to-ptr)
          };
        } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
          // If an access violation occurs, then the shared memory couldn't be properly read.
          // Set the version data to an error instead of crashing.
          m_versionData = Err(DracError(PlatformSpecific, "Failed to read kernel version from KUSER_SHARED_DATA"));
        }
  #pragma clang diagnostic pop
      }

      ~OsVersionCache() = default;
    };

    // Captures a process tree and stores it for later reuse.
    class ProcessTreeCache {
     public:
      struct Data {
        DWORD  parentPid = 0;
        String baseExeNameLower;
      };

      static fn getInstance() -> ProcessTreeCache& {
        static ProcessTreeCache Instance;
        return Instance;
      }

      fn initialize() -> Result<> {
        // Ensure exclusive access to the initialization process.
        LockGuard lock(m_initMutex);

        // Prevent wasteful re-initialization if the cache is already populated.
        if (m_initialized)
          return {};

        // Use the Toolhelp32Snapshot API to get a snapshot of all running processes.
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
          return Err(DracError(PlatformSpecific, "Failed to create snapshot of processes"));

        // This structure must be initialized with its own size before use; it's a WinAPI requirement.
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        // Iterate through all processes captured in the snapshot.
        for (BOOL ok = Process32FirstW(hSnap, &pe32); ok; ok = Process32NextW(hSnap, &pe32)) {
          std::filesystem::path exePath(pe32.szExeFile);
          std::wstring          baseName = exePath.stem().wstring();

          // Normalize for case-insensitive comparison.
          std::ranges::transform(baseName, baseName.begin(), [](const wchar_t character) { return towlower(character); });

          const Result<String> baseNameUTF8 = helpers::ConvertWStringToUTF8(baseName);

          if (!baseNameUTF8)
            continue;

          m_processMap[pe32.th32ProcessID] = Data {
            .parentPid        = pe32.th32ParentProcessID,
            .baseExeNameLower = *baseNameUTF8
          };
        }

        // Ensure that the handle is closed to avoid leaks.
        CloseHandle(hSnap);
        m_initialized = true;

        return {};
      }

      fn getProcessMap() const -> const std::unordered_map<DWORD, Data>& {
        return m_processMap;
      }

      ProcessTreeCache(const ProcessTreeCache&)                = delete;
      ProcessTreeCache(ProcessTreeCache&&)                     = delete;
      fn operator=(const ProcessTreeCache&)->ProcessTreeCache& = delete;
      fn operator=(ProcessTreeCache&&)->ProcessTreeCache&      = delete;

     private:
      std::unordered_map<DWORD, Data> m_processMap;
      bool                            m_initialized = false;
      Mutex                           m_initMutex;

      ProcessTreeCache()  = default;
      ~ProcessTreeCache() = default;
    };
  } // namespace cache

  namespace shell {
    template <usize sz>
    fn FindShellInProcessTree(const DWORD startPid, const Array<Pair<StringView, StringView>, sz>& shellMap) -> Result<String> {
      using cache::ProcessTreeCache;
      // PID 0 (System Idle Process) is always the root process, and cannot have a parent.
      if (startPid == 0)
        return Err(DracError(PlatformSpecific, "Start PID is 0"));

      if (!ProcessTreeCache::getInstance().initialize())
        return Err(DracError(PlatformSpecific, "Failed to initialize process tree cache"));

      const UnorderedMap<DWORD, ProcessTreeCache::Data>& processMap = ProcessTreeCache::getInstance().getProcessMap();

      DWORD currentPid = startPid;

      // This is a pretty reasonable depth and should cover most cases without excessive recursion.
      constexpr i32 maxDepth = 16;

      i32 depth = 0;

      while (currentPid != 0 && depth < maxDepth) {
        auto procIt = processMap.find(currentPid);
        if (procIt == processMap.end())
          break;

        // Get the lowercase name of the process.
        const String& processName = procIt->second.baseExeNameLower;

        // Check if the process name matches any shell in the map,
        // and return its friendly-name counterpart if it is.
        if (const auto mapIter = std::ranges::find_if(shellMap, [&](const Pair<StringView, StringView>& pair) { return StringView { processName } == pair.first; });
            mapIter != std::ranges::end(shellMap))
          return String(mapIter->second);

        // Move up the tree to the parent process.
        currentPid = procIt->second.parentPid;
        depth++;
      }

      return Err(DracError(NotFound, "Shell not found"));
    }
  } // namespace shell
} // namespace

namespace draconis::core::system {
  using namespace cache;
  using namespace constants;
  using namespace helpers;

  fn GetMemInfo() -> Result<ResourceUsage> {
    // Passed to GlobalMemoryStatusEx to retrieve memory information.
    // dwLength is required to be set as per WinAPI.
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);

    if (GlobalMemoryStatusEx(&memInfo))
      return ResourceUsage { .usedBytes = memInfo.ullTotalPhys - memInfo.ullAvailPhys, .totalBytes = memInfo.ullTotalPhys };

    return Err(DracError(PlatformSpecific, std::format("GlobalMemoryStatusEx failed with error code {}", GetLastError())));
  }

  #if DRAC_ENABLE_NOWPLAYING
  fn GetNowPlaying() -> Result<MediaInfo> {
    // WinRT makes HEAVY use of namespaces and has very long names for
    // structs and classes, so its easier to alias most things.
    using namespace winrt::Windows::Media::Control;
    using namespace winrt::Windows::Foundation;

    using Session         = GlobalSystemMediaTransportControlsSession;
    using SessionManager  = GlobalSystemMediaTransportControlsSessionManager;
    using MediaProperties = GlobalSystemMediaTransportControlsSessionMediaProperties;

    try {
      // WinRT provides a nice easy way to get the current media session.
      // AFAIK, there's no other way to easily get the current media session on Windows.

      // getNowPlaying() isn't async, so we just RequestAsync() and immediately get() the result.
      const SessionManager sessionManager = SessionManager::RequestAsync().get();

      if (const Session currentSession = sessionManager.GetCurrentSession()) {
        // TryGetMediaPropertiesAsync() is also async, so we have to get() the result.
        const MediaProperties mediaProperties = currentSession.TryGetMediaPropertiesAsync().get();

        return MediaInfo(winrt::to_string(mediaProperties.Title()), winrt::to_string(mediaProperties.Artist()));
      }

      return Err(DracError(NotFound, "No media session found"));
    } catch (const winrt::hresult_error& e) {
      // Make sure to catch any errors that WinRT might throw.
      return Err(DracError(e));
    }
  }
  #endif // DRAC_ENABLE_NOWPLAYING

  fn GetOSVersion() -> Result<String> {
    // Windows is weird about its versioning scheme, and Windows 11 is still
    // considered Windows 10 in the registry. We have to manually check if
    // the actual version is Windows 11 by checking the build number.
    constexpr const wchar_t* windows10 = L"Windows 10";
    constexpr const wchar_t* windows11 = L"Windows 11";

    constexpr usize windowsLen = std::char_traits<wchar_t>::length(windows10);

    const RegistryCache& registry = RegistryCache::getInstance();

    HKEY currentVersionKey = registry.getCurrentVersionKey();

    if (!currentVersionKey)
      return Err(DracError(NotFound, "Failed to open registry key"));

    Result<std::wstring> productName = GetRegistryValue(currentVersionKey, PRODUCT_NAME);

    if (!productName)
      return Err(productName.error());

    if (productName->empty())
      return Err(DracError(NotFound, "ProductName not found in registry"));

    // Build 22000+ of Windows are all considered Windows 11, so we can safely replace the product name
    // if it's currently "Windows 10" and the build number is greater than or equal to 22000.
    if (const Result<u64> buildNumberOpt = OsVersionCache::getInstance().getBuildNumber())
      if (const u64 buildNumber = *buildNumberOpt; buildNumber >= 22000)
        if (const size_t pos = productName->find(windows10); pos != std::wstring::npos) {
          // Make sure we're not replacing a substring of a larger string. Should never happen,
          // but if it ever does, we'll just leave the product name unchanged.
          const bool startBoundary = (pos == 0 || !iswalnum(productName->at(pos - 1)));
          const bool endBoundary   = (pos + windowsLen == productName->length() || !iswalnum(productName->at(pos + windowsLen)));

          if (startBoundary && endBoundary)
            productName->replace(pos, windowsLen, windows11);
        }

    // Append the display version if it exists.
    const Result<std::wstring> displayVersion = GetRegistryValue(currentVersionKey, DISPLAY_VERSION);

    if (!displayVersion)
      return Err(displayVersion.error());

    const Result<String> productNameUTF8 = ConvertWStringToUTF8(*productName);

    if (!productNameUTF8)
      return Err(productNameUTF8.error());

    if (displayVersion->empty())
      return *productNameUTF8;

    const Result<String> displayVersionUTF8 = ConvertWStringToUTF8(*displayVersion);

    if (!displayVersionUTF8)
      return Err(displayVersionUTF8.error());

    return *productNameUTF8 + " " + *displayVersionUTF8;
  }

  fn GetHost() -> Result<String> {
    // See the RegistryCache class for how the registry keys are retrieved.
    const RegistryCache& registry = RegistryCache::getInstance();

    HKEY hardwareConfigKey = registry.getHardwareConfigKey();

    if (!hardwareConfigKey)
      return Err(DracError(NotFound, "Failed to open registry key"));

    const Result<std::wstring> systemFamily = GetRegistryValue(hardwareConfigKey, SYSTEM_FAMILY);

    if (!systemFamily)
      return Err(DracError(NotFound, "SystemFamily not found in registry"));

    return ConvertWStringToUTF8(*systemFamily);
  }

  fn GetKernelVersion() -> Result<String> {
    // See the OsVersionCache class for how the version data is retrieved.
    const Result<OsVersionCache::VersionData>& versionDataResult = OsVersionCache::getInstance().getVersionData();

    if (!versionDataResult)
      return Err(versionDataResult.error());

    const auto& [majorVersion, minorVersion, buildNumber] = *versionDataResult;

    return std::format("{}.{}.{}", majorVersion, minorVersion, buildNumber);
  }

  fn GetWindowManager() -> Result<String> {
    if (!cache::ProcessTreeCache::getInstance().initialize())
      return Err(DracError(PlatformSpecific, "Failed to initialize process tree cache"));

    for (const auto& [parentPid, baseExeNameLower] : cache::ProcessTreeCache::getInstance().getProcessMap() | std::views::values) {
      const StringView processName = baseExeNameLower;

      if (const auto mapIter = std::ranges::find_if(windowManagerMap, [&](const Pair<StringView, StringView>& pair) { return processName == pair.first; }); mapIter != std::ranges::end(windowManagerMap))
        return String(mapIter->second);
    }

    return "DWM";
  }

  fn GetDesktopEnvironment() -> Result<String> {
    // Windows doesn't really have the concept of a desktop environment,
    // so our next best bet is just displaying the UI design based on the build number.

    const Result<u64> buildResult = OsVersionCache::getInstance().getBuildNumber();

    if (!buildResult)
      return Err(buildResult.error());

    const u64 build = *buildResult;

    if (build >= 15063)
      return "Fluent";

    if (build >= 9200)
      return "Metro";

    if (build >= 6000)
      return "Aero";

    return "Classic";
  }

  fn GetShell() -> Result<String> {
    using draconis::utils::env::GetEnv;
    using shell::FindShellInProcessTree;

    // MSYS2 environments automatically set the MSYSTEM environment variable.
    if (const Result<String> msystemResult = GetEnv("MSYSTEM"); msystemResult && !msystemResult->empty()) {
      String shellPath;

      // The SHELL environment variable should basically always be set.
      if (const Result<String> shellResult = GetEnv("SHELL"); shellResult && !shellResult->empty())
        shellPath = *shellResult;

      if (!shellPath.empty()) {
        // Get the executable name from the path.
        const usize lastSlash = shellPath.find_last_of("\\/");
        String      shellExe  = (lastSlash != String::npos) ? shellPath.substr(lastSlash + 1) : shellPath;

        std::ranges::transform(shellExe, shellExe.begin(), [](const u8 character) { return std::tolower(character); });

        // Remove the .exe extension if it exists.
        if (shellExe.ends_with(".exe"))
          shellExe.resize(shellExe.length() - 4);

        // Check if the executable name matches any shell in the map.
        if (const auto iter = std::ranges::find_if(msysShellMap, [&](const Pair<StringView, StringView>& pair) { return StringView { shellExe } == pair.first; }); iter != std::ranges::end(msysShellMap))
          return String(iter->second);

        // If the executable name doesn't match any shell in the map, we might as well just return it as is.
        return shellExe;
      }

      // If the SHELL environment variable is not set, we can fall back to checking the process tree.
      // This is slower, but if we don't have the SHELL variable there's not much else we can do.
      const Result<String> msysShell = FindShellInProcessTree(GetCurrentProcessId(), msysShellMap);

      if (msysShell)
        return *msysShell;

      return Err(msysShell.error());
    }

    // Normal windows shell environments don't set any environment variables we can check,
    // so we have to check the process tree instead.
    const Result<String> windowsShell = FindShellInProcessTree(GetCurrentProcessId(), windowsShellMap);

    if (windowsShell)
      return *windowsShell;

    return Err(windowsShell.error());
  }

  fn GetDiskUsage() -> Result<ResourceUsage> {
    // GetDiskFreeSpaceExW is a pretty old function and doesn't use native 64-bit integers,
    // so we have to use ULARGE_INTEGER instead. It's basically a union that holds either a
    // 64-bit integer or two 32-bit integers.
    ULARGE_INTEGER freeBytes, totalBytes;

    // Get the disk usage for the C: drive.
    if (FAILED(GetDiskFreeSpaceExW(L"C:\\\\", nullptr, &totalBytes, &freeBytes)))
      return Err(DracError(PlatformSpecific, "Failed to get disk usage"));

    // Calculate the used bytes by subtracting the free bytes from the total bytes.
    // QuadPart corresponds to the 64-bit integer in the union. (LowPart/HighPart are for the 32-bit integers.)
    return ResourceUsage { .usedBytes = totalBytes.QuadPart - freeBytes.QuadPart, .totalBytes = totalBytes.QuadPart };
  }

  fn GetCPUModel() -> Result<String> {
    /*
     * This function attempts to get the CPU model name on Windows in two ways:
     * 1. Using __cpuid on x86/x86_64 platforms (much more direct and efficient).
     * 2. Reading from the registry on all platforms (slower, but more reliable).
     */
  #if DRAC_ARCH_X86_64 || DRAC_ARCH_I686
    {
      /*
       * The CPUID instruction is used to get the CPU model name on x86/x86_64 platforms.
       * 1. First, we call CPUID with leaf 0x80000000 to ask the CPU if it supports the
       *    extended functions needed to retrieve the brand string.
       * 2. If it does, we then make three more calls to retrieve the 48-byte brand
       *    string, which the CPU provides in three 16-byte chunks.
       *
       * (In this context, a "leaf" is basically just an action we ask the CPU to perform.)
       */

      // Array to hold the raw 32-bit values from the EAX, EBX, ECX, and EDX registers.
      Array<i32, 4> cpuInfo = {};

      // Buffer to hold the raw 48-byte brand string + null terminator.
      Array<char, 49> brandString = {};

      // Step 1: Check for brand string support. The result is returned in EAX (cpuInfo[0]).
      __cpuid(0x80000000, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);

      // We must have extended functions support (functions up to 0x80000004).
      if (const u32 maxFunction = cpuInfo[0]; maxFunction >= 0x80000004) {
        // Retrieve the brand string in three 16-byte parts.
        for (u32 i = 0; i < 3; i++) {
          // Call leaves 0x80000002, 0x80000003, and 0x80000004. Each call
          // returns a 16-byte chunk of the brand string.
          __cpuid(0x80000002 + i, cpuInfo[0], cpuInfo[1], cpuInfo[2], cpuInfo[3]);

          // Copy the chunk into the brand string buffer.
          std::memcpy(&brandString.at(i * 16), cpuInfo.data(), sizeof(cpuInfo));
        }

        String result(brandString.data());

        // Clean up any possible trailing whitespace.
        while (!result.empty() && std::isspace(result.back()))
          result.pop_back();

        // We're done if we got a valid, non-empty string.
        // Otherwise, fallback to querying the registry.
        if (!result.empty())
          return result;
      }
    }
  #endif // DRAC_ARCH_X86_64 || DRAC_ARCH_I686

    {
      /*
       * If the CPUID instruction fails/is unsupported on the target architecture,
       * we fallback to querying the registry. This is a lot more reliable than
       * querying the CPU itself and supports all architectures, but it's also slower.
       */

      HKEY hKey = nullptr;

      // This key contains information about the processor.
      if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        // Get the processor name value from the registry key.
        Result<std::wstring> processorNameW = GetRegistryValue(hKey, L"ProcessorNameString");

        // Ensure the key is closed to prevent leaks.
        RegCloseKey(hKey);

        // The registry returns wide strings so we have to convert to UTF-8 before returning.
        if (processorNameW)
          return ConvertWStringToUTF8(*processorNameW);
      }
    }

    // At this point, there's no other good method to get the CPU model on Windows.
    // Using WMI is useless because it just calls the same registry key we're already using.
    return Err(DracError(NotFound, "All methods to get CPU model failed on this platform"));
  }

  fn GetGPUModel() -> Result<String> {
    // Used to create and enumerate DirectX graphics interfaces.
    IDXGIFactory* pFactory = nullptr;

    // The __uuidof operator is a Microsoft-specific extension that gets the GUID of a COM interface.
    // It's required by CreateDXGIFactory. The pragma below disables the compiler warning about this
    // non-standard extension, as its use is necessary here.
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wlanguage-extension-token"
    // NOLINTNEXTLINE(*-pro-type-reinterpret-cast) - CreateDXGIFactory needs a void** parameter, not an IDXGIFactory**.
    if (FAILED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pFactory))))
      return Err(DracError(PlatformSpecific, "Failed to create DXGI Factory"));
  #pragma clang diagnostic pop

    // Attempt to get the first adapter.
    IDXGIAdapter* pAdapter = nullptr;

    // 0 = primary adapter/GPU
    if (pFactory->EnumAdapters(0, &pAdapter) == DXGI_ERROR_NOT_FOUND) {
      // Clean up factory.
      pFactory->Release();

      return Err(DracError(NotFound, "No DXGI adapters found"));
    }

    // Get the adapter description.
    DXGI_ADAPTER_DESC desc {};

    if (FAILED(pAdapter->GetDesc(&desc))) {
      // Make sure to release the adapter and factory if GetDesc fails.
      pAdapter->Release();
      pFactory->Release();

      return Err(DracError(PlatformSpecific, "Failed to get adapter description"));
    }

    // The DirectX description is a wide string.
    // We have to convert it to a UTF-8 string.
    Array<char, 128> gpuName {};

    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, gpuName.data(), gpuName.size(), nullptr, nullptr);

    // Clean up resources.
    pAdapter->Release();
    pFactory->Release();

    return gpuName.data();
  }
} // namespace draconis::core::system

  #if DRAC_ENABLE_PACKAGECOUNT
namespace draconis::services::packages {
  using draconis::utils::env::GetEnvW;
  using helpers::GetDirCount;

  fn CountChocolatey() -> Result<u64> {
    // C:\ProgramData\chocolatey is the default installation directory.
    std::wstring chocoPath = L"C:\\ProgramData\\chocolatey";

    // If the ChocolateyInstall environment variable is set, use that instead.
    // Most of the time it's set to C:\ProgramData\chocolatey, but it can be overridden.
    if (const Result<wchar_t*> chocoEnv = GetEnvW(L"ChocolateyInstall"); chocoEnv)
      chocoPath = *chocoEnv;

    // The lib directory contains the package metadata.
    chocoPath.append(L"\\lib");

    // Get the number of directories in the lib directory.
    // This corresponds to the number of packages installed.
    if (Result<u64> dirCount = GetDirCount(chocoPath))
      return *dirCount;

    return Err(DracError(NotFound, "Failed to get Chocolatey package count"));
  }

  fn CountScoop() -> Result<u64> {
    std::wstring scoopAppsPath;

    // The SCOOP environment variable should be used first if it's set.
    if (const Result<wchar_t*> scoopEnv = GetEnvW(L"SCOOP"); scoopEnv) {
      scoopAppsPath = *scoopEnv;
      scoopAppsPath.append(L"\\apps");
    } else if (const Result<wchar_t*> userProfile = GetEnvW(L"USERPROFILE"); userProfile) {
      // Otherwise, we can try finding the scoop folder in the user's home directory.
      scoopAppsPath = *userProfile;
      scoopAppsPath.append(L"\\scoop\\apps");
    } else {
      // The user likely doesn't have scoop installed if neither of those other methods work.
      return Err(DracError(NotFound, "Could not determine Scoop installation directory (SCOOP and USERPROFILE environment variables not found)"));
    }

    // Get the number of directories in the apps directory.
    // This corresponds to the number of packages installed.
    if (Result<u64> dirCount = GetDirCount(scoopAppsPath))
      return *dirCount;

    return Err(DracError(NotFound, "Failed to get Scoop package count"));
  }

  fn CountWinGet() -> Result<u64> {
    try {
      using winrt::Windows::Management::Deployment::PackageManager;

      // The only good way to get the number of packages installed via winget is using WinRT.
      // It's a bit slow, but it's still faster than shelling out to the command line.
      // FindPackagesForUser returns an iterator to the first package, so we can use std::ranges::distance to get the number of packages.
      return std::ranges::distance(PackageManager().FindPackagesForUser(L""));
    } catch (const winrt::hresult_error& e) {
      // Make sure to catch any errors that WinRT might throw.
      return Err(DracError(e));
    }
  }
} // namespace draconis::services::packages
  #endif // DRAC_ENABLE_PACKAGECOUNT

#endif // _WIN32
