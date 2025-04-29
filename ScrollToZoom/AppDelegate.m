/*
 *  AppDelegate.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "AppDelegate.h"
#import "STZScrollToZoom.h"
#import "STZSettings.h"
#import "STZWindow.h"
#import "STZProcessManager.h"
#import "GeneratedAssetSymbols.h"


static NSString *const REPO_URL_PATH = @"https://github.com/alphaArgon/ScrollToZoom";


@implementation AppDelegate {
    NSStatusItem *_statusItem;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyAccessory];

    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    [[_statusItem button] setImage:[NSImage imageNamed:ACImageNameStatusIcon]];

    NSArray *nibObjects;
    [[NSBundle mainBundle] loadNibNamed:@"StatusMenu" owner:self topLevelObjects:&nibObjects];

    for (id object in nibObjects) {
        if ([object isKindOfClass:[NSMenu self]]) {
            [_statusItem setMenu:object];
            break;
        }
    }

    STZLoadArgumentsFromUserDefaults();
    if (!STZSetScrollToZoomEnabled(true)) {
        [STZWindow orderFrontSharedWindow];
    }
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag {
    [STZWindow orderFrontSharedWindow];
    return NO;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return !STZGetScrollToZoomEnabled();
}

- (void)orderFrontAboutPanel:(id)sender {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [[NSApplication sharedApplication] orderFrontStandardAboutPanelWithOptions:@{
        NSAboutPanelOptionCredits: [[NSAttributedString alloc] initWithString:NSLocalizedString(@"view-source-on-github", nil)
                                                                   attributes:@{
            NSLinkAttributeName: [NSURL URLWithString:REPO_URL_PATH],
            NSFontAttributeName: [NSFont toolTipsFontOfSize:12]
        }]
    }];
}

- (void)orderFrontSharedWindow:(id)sender {
    [STZWindow orderFrontSharedWindow];
}

static NSRunningApplication *keyApplication(void) {
    return [[NSWorkspace sharedWorkspace] frontmostApplication] ?: [NSRunningApplication currentApplication];
}

static void toggleSTZEventTapOptionsForKeyApplication(STZEventTapOptions flag) {
    NSRunningApplication *app = keyApplication();
    CFStringRef bundleID = (__bridge void *)[app bundleIdentifier];
    STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier(bundleID);
    if (options & flag) {
        options &= ~flag;
    } else {
        options |= flag;
    }
    STZSetEventTapOptionsForBundleIdentifier(bundleID, options);
}

- (void)toggleEnabledForKeyApplication:(id)sender {
    toggleSTZEventTapOptionsForKeyApplication(kSTZEventTapDisabled);
}

- (void)toggleExcludingFlagsForKeyApplication:(id)sender {
    toggleSTZEventTapOptionsForKeyApplication(kSTZEventTapExcludeFlags);
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if ([menuItem action] == @selector(toggleEnabledForKeyApplication:)) {
        NSRunningApplication *app = keyApplication();
        [menuItem setTitle:[NSString stringWithFormat:NSLocalizedString(@"enabled-for-%@", nil), [app localizedName]]];
        CFStringRef bundleID = (__bridge void *)[app bundleIdentifier];
        STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier(bundleID);
        [menuItem setState:!(options & kSTZEventTapDisabled)];
        return YES;
    }

    if ([menuItem action] == @selector(toggleExcludingFlagsForKeyApplication:)) {
        NSRunningApplication *app = keyApplication();
        CFStringRef bundleID = (__bridge void *)[app bundleIdentifier];
        STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier(bundleID);
        [menuItem setState:!!(options & kSTZEventTapExcludeFlags)];
        return !(options & kSTZEventTapDisabled);
    }

    return YES;
}

@end
