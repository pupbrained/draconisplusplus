#pragma once

#ifdef __APPLE__
#ifdef __OBJC__
#import <Foundation/Foundation.h>

@interface Bridge : NSObject
+ (NSDictionary*)currentPlayingMetadata;
+ (NSString*)macOSVersion;
@end
#else
extern "C" {
  const char* GetCurrentPlayingTitle();
  const char* GetCurrentPlayingArtist();
  const char* GetMacOSVersion();
}
#endif
#endif
