#pragma once

#ifdef __APPLE__
#ifdef __OBJC__
#import <Foundation/Foundation.h>

@interface NowPlayingBridge : NSObject
+ (NSDictionary*)currentPlayingMetadata;
@end
#else
extern "C" {
  const char* GetCurrentPlayingTitle();
  const char* GetCurrentPlayingArtist();
}
#endif
#endif
