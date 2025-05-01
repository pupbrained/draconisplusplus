#pragma once

#ifdef __APPLE__

// clang-format off
#include "src/core/util/defs.hpp"
#include "src/core/util/error.hpp"
#include "src/core/util/types.hpp"
// clang-format on
using util::error::DracError;
using util::types::MediaInfo, util::types::String, util::types::Result;

  #ifdef __OBJC__
    #import <Foundation/Foundation.h>

@interface Bridge : NSObject
+ (void)fetchCurrentPlayingMetadata:(void (^)(Result<NSDictionary*, const char*>))completion;
+ (Result<String, DracError>)macOSVersion;
@end
  #else
extern "C++" {
  fn GetCurrentPlayingInfo() -> Result<MediaInfo, DracError>;
  fn GetMacOSVersion() -> Result<String, DracError>;
}
  #endif
#endif
