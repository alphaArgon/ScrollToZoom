/*
 *  MGZWindow.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN


@interface MGZWindow : NSWindow

@property(nonatomic, class, readonly) MGZWindow *sharedWindow;

+ (void)orderFrontSharedWindow;

@end


NS_ASSUME_NONNULL_END
