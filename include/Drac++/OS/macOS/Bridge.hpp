#pragma once

#ifdef __APPLE__

// clang-format off
#include "DracUtils/Definitions.hpp"
#include "DracUtils/Error.hpp"
#include "DracUtils/Types.hpp"

#ifdef __OBJC__
  #import <Foundation/Foundation.h>

  @interface Bridge : NSObject
  + (void)fetchCurrentPlayingMetadata:(void (^_Nonnull)(NSDictionary* __nullable, NSError* __nullable))completion;
  @end
#else
  extern "C++" {
    fn GetCurrentPlayingInfo() -> util::types::Result<util::types::MediaInfo>;
  }
#endif
// clang-format on

#endif
