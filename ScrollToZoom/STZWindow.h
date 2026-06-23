/*
 *  STZWindow.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN


@interface STZWindow : NSWindow

@property(nonatomic, class, readonly) STZWindow *sharedWindow;

+ (void)orderFrontSharedWindowWithAdvanceSettings:(BOOL)advanced;

@end


NS_ASSUME_NONNULL_END
