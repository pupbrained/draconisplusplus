#ifdef __SWITCH__

  #include <switch.h>
  #include <switch/kernel/detect.h>
  #include <switch/services/nifm.h>
  #include <switch/services/set.h>
  #include <switch/services/spl.h>
  #include <switch/services/time.h>

  #include <Drac++/Core/System.hpp>
  #include <Drac++/Services/Weather.hpp>

  #include <Drac++/Utils/CacheManager.hpp>

auto main() -> s32 {
  consoleInit(nullptr);
  Result timeRes = timeInitialize();
  if (R_FAILED(timeRes))
    return static_cast<s32>(timeRes);

  // Initialize network for weather services
  #if DRAC_ENABLE_WEATHER
  Result nifmRes = nifmInitialize(NifmServiceType_User);
  if (R_FAILED(nifmRes)) {
    std::println("Failed to initialize network: 0x{:08X}", nifmRes);
    // Continue without network - weather will fail gracefully
  }
  #endif

  padConfigureInput(1, HidNpadStyleSet_NpadStandard);

  PadState pad;
  padInitializeDefault(&pad);

  draconis::utils::cache::CacheManager cache;

  // Initialize weather service (using MetNo provider, coordinates for Tokyo, metric units)
  draconis::services::weather::UniquePointer<draconis::services::weather::IWeatherService> weatherService;
  #if DRAC_ENABLE_WEATHER
  weatherService = draconis::services::weather::CreateWeatherService(
    draconis::services::weather::Provider::MetNo,
    draconis::services::weather::Coords(35.6762, 139.6503), // Tokyo coordinates
    draconis::services::weather::UnitSystem::Metric
  );
  #endif

  using draconis::core::system::GetBatteryInfo;
  using draconis::core::system::GetCPUCores;
  using draconis::core::system::GetCPUModel;
  using draconis::core::system::GetGPUModel;
  using draconis::core::system::GetKernelVersion;
  using draconis::core::system::GetOperatingSystem;

  if (auto osRes = GetOperatingSystem(cache); osRes)
    std::println("\x1b[1;0HFirmware: {} {}", osRes->name, osRes->version);
  else
    std::println("Failed to get firmware version: {}", osRes.error().message);

  if (auto cpuModel = GetCPUModel(cache); cpuModel)
    std::println("\x1b[2;0HCPU Model: {}", *cpuModel);
  else
    std::println("Failed to get CPU model: {}", cpuModel.error().message);

  if (auto cores = GetCPUCores(cache); cores)
    std::println("\x1b[3;0HCPU Cores available: {}", cores->logical);
  else
    std::println("Failed to get CPU cores: {}", cores.error().message);

  if (auto kernel = GetKernelVersion(cache); kernel)
    std::println("\x1b[4;0HKernel: {}", *kernel);
  else
    std::println("Failed to get kernel version: {}", kernel.error().message);

  if (auto gpu = GetGPUModel(cache); gpu)
    std::println("\x1b[5;0HGPU: {}", *gpu);
  else
    std::println("Failed to get GPU model: {}", gpu.error().message);

  u64       frameCounter      = 0;
  const u64 FRAMES_PER_UPDATE = 60;

  // Weather cache variables
  std::optional<draconis::services::weather::Report> cachedWeather;
  u64                                                lastWeatherCheck        = 0;
  const u64                                          WEATHER_UPDATE_INTERVAL = 600; // Update weather every 10 seconds (600 frames at 60fps)

  while (appletMainLoop()) {
    padUpdate(&pad);

    u64 kDown = padGetButtonsDown(&pad);

    if ((kDown & HidNpadButton_Plus) != 0)
      break;

    if (frameCounter % FRAMES_PER_UPDATE == 0) {
      u64 currentTime = 0;
      if (R_SUCCEEDED(timeGetCurrentTime(TimeType_UserSystemClock, &currentTime))) {
        TimeCalendarTime cal = {};
        if (R_SUCCEEDED(timeToCalendarTimeWithMyRule(currentTime, &cal, nullptr)))
          std::println("\x1b[11;0HDate/Time: {:04}-{:02}-{:02} {:02}:{:02}:{:02}", cal.year, cal.month, cal.day, cal.hour, cal.minute, cal.second);

        if (auto mem = draconis::core::system::GetMemInfo(cache); mem)
          std::println("\x1b[10;0HMemory usage: {:.2f} MiB", static_cast<double>(mem->usedBytes) / (1024.0 * 1024.0));

        if (auto bat = GetBatteryInfo(cache); bat) {
          using enum draconis::utils::types::Battery::Status;

          const char* statusStr = "Unknown";

          switch (bat->status) {
            case Charging:
              statusStr = "Charging";
              break;
            case Discharging:
              statusStr = "Discharging";
              break;
            case Full:
              statusStr = "Full";
              break;
            case NotPresent:
              statusStr = "N/A";
              break;
            default:
              break;
          }

          if (bat->percentage)
            std::println("\x1b[12;0HBattery: {}% ({})   ", *bat->percentage, statusStr);
          else
            std::println("\x1b[12;0HBattery: -- ({})   ", statusStr);
        } else
          std::println("\x1b[12;0HBattery: Error ({})   ", bat.error().message);

        // Weather display logic
  #if DRAC_ENABLE_WEATHER
        if (weatherService && (frameCounter - lastWeatherCheck >= WEATHER_UPDATE_INTERVAL || !cachedWeather)) {
          if (auto weatherResult = weatherService->getWeatherInfo(); weatherResult) {
            cachedWeather    = *weatherResult;
            lastWeatherCheck = frameCounter;
          }
        }

        if (cachedWeather) {
          std::println("\x1b[13;0HWeather: {:.1f}°C, {}", cachedWeather->temperature, cachedWeather->description);
        } else {
          std::println("\x1b[13;0HWeather: No data available");
        }
  #else
        std::println("\x1b[13;0HWeather: Not enabled");
  #endif
      }
    }

    frameCounter++;
    consoleUpdate(nullptr);
  }

  consoleExit(nullptr);
  timeExit();

  #if DRAC_ENABLE_WEATHER
  nifmExit();
  #endif

  return 0;
}

#endif