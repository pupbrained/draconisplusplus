#pragma once

#include "src/config/config.h"
#include "src/util/types.h"

struct BytesToGiB {
  u64 value;
};

constexpr u64 GIB = 1'073'741'824;

template <>
struct std::formatter<BytesToGiB> : std::formatter<double> {
  fn format(const BytesToGiB& BTG, auto& ctx) const {
    return std::format_to(ctx.out(), "{:.2f}GiB", static_cast<f64>(BTG.value) / GIB);
  }
};

// Structure to hold the collected system data
struct SystemData {
  String                                  date;
  String                                  host;
  String                                  kernel_version;
  Result<String, String>                  os_version;
  Result<u64, String>                     mem_info;
  Option<String>                          desktop_environment;
  String                                  window_manager;
  Option<Result<String, NowPlayingError>> now_playing;
  Option<WeatherOutput>                   weather_info;
  u64                                     disk_used;
  u64                                     disk_total;
  String                                  shell;

  // Static function to fetch the data
  static fn fetchSystemData(const Config& config) -> SystemData;
};
