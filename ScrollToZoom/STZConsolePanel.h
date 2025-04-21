/*
 *  STZConsolePanel.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/8.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN


@interface STZConsolePanel : NSWindow

@property(nonatomic, class, readonly) STZConsolePanel *sharedPanel;

+ (void)orderFrontSharedPanel;

- (void)addLog:(NSString *)message;
- (void)clearLogs:(nullable id)sender;
- (void)toggleLoggingPaused:(nullable id)sender;
- (BOOL)isLoggingPaused;

@end


NS_ASSUME_NONNULL_END
