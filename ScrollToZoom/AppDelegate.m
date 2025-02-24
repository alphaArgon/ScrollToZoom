/*
 *  AppDelegate.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "AppDelegate.h"
#import "STZEventTap.h"
#import "STZWindow.h"


@implementation AppDelegate {
    NSStatusItem *_statusItem;
}

- (void)orderFrontSharedWindow:(id)sender {
    [STZWindow orderFrontSharedWindow];
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyAccessory];

    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    [[_statusItem button] setImage:[NSImage imageNamed:@"StatusIcon"]];

    NSArray *nibObjects;
    [[NSBundle mainBundle] loadNibNamed:@"StatusMenu" owner:self topLevelObjects:&nibObjects];

    for (id object in nibObjects) {
        if ([object isKindOfClass:[NSMenu self]]) {
            [_statusItem setMenu:object];
            break;
        }
    }

    [STZWindow orderFrontSharedWindowIfNeeded];
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag {
    [STZWindow orderFrontSharedWindow];
    return NO;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return !STZGetEventTapEnabled();
}

@end
