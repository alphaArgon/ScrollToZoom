/*
 *  AppDelegate.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "AppDelegate.h"
#import "STZEventHandling.h"
#import "STZProcessManager.h"
#import "STZWindow.h"
#import "STZConsolePanel.h"
#import "GeneratedAssetSymbols.h"


static NSString *const REPO_URL_PATH = @"https://github.com/alphaArgon/ScrollToZoom";
static NSUInteger const HEADER_ITEM_TAG = 110105;


@implementation AppDelegate {
    NSStatusItem *_statusItem;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyAccessory];

    _statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];

    NSArray *nibObjects;
    [[NSBundle mainBundle] loadNibNamed:@"StatusMenu" owner:self topLevelObjects:&nibObjects];

    for (id object in nibObjects) {
        if ([object isKindOfClass:[NSMenu self]]) {
            if (@available(macOS 14, *)) {
                NSInteger headerItemIndex = [(NSMenu *)object indexOfItemWithTag:HEADER_ITEM_TAG];
                if (headerItemIndex != -1) {
                    NSString *header = [[(NSMenu *)object itemAtIndex:headerItemIndex] title];
                    NSMenuItem *headerItem = [NSMenuItem sectionHeaderWithTitle:header];
                    [headerItem setTag:HEADER_ITEM_TAG];
                    [(NSMenu *)object removeItemAtIndex:headerItemIndex];
                    [(NSMenu *)object insertItem:headerItem atIndex:headerItemIndex];
                }
            }
            [_statusItem setMenu:object];
            break;
        }
    }

    if (!STZSetWorkingModes(STZGetPreferredModes())) {
        [STZWindow orderFrontSharedWindowWithAdvanceSettings:NO];
    }

    [self updateStatusItem:nil];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(updateStatusItem:)
                                                 name:(__bridge id)kSTZWorkingModesDidChangeNotification
                                               object:nil];

#if DEBUG
    if (([NSEvent modifierFlags] & (NSEventModifierFlagOption))) {
        [STZConsolePanel orderFrontSharedPanel];
    }
#endif
}

- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)flag {
    BOOL optionDown = !!([NSEvent modifierFlags] & NSEventModifierFlagOption);
    [STZWindow orderFrontSharedWindowWithAdvanceSettings:optionDown];
    return NO;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    STZModes modes = STZGetWorkingModes();
    return !(modes & kSTZPracticalModesMask);
}

- (void)updateStatusItem:(id)sender {
    STZModes modes = STZGetWorkingModes();
    if (!(modes & kSTZPracticalModesMask)) {
        [[_statusItem button] setImage:[NSImage imageNamed:ACImageNameStatusIconDisabled]];
    } else if (modes & kSTZTriggerFlagsEnabled) {
        [[_statusItem button] setImage:[NSImage imageNamed:ACImageNameStatusIcon]];
    } else {
        [[_statusItem button] setImage:[NSImage imageNamed:ACImageNameStatusIconMagic]];
    }

    bool hideItems = !(modes & kSTZTriggerFlagsEnabled);
    for (NSMenuItem *item in [[_statusItem menu] itemArray]) {
        if ([item tag] == HEADER_ITEM_TAG
         || [item action] == @selector(toggleEnabledForKeyApplication:)
         || [item action] == @selector(toggleExcludingFlagsForKeyApplication:)) {
            [item setHidden:hideItems];
        }
    }
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
    [STZWindow orderFrontSharedWindowWithAdvanceSettings:NO];
}

- (void)orderFrontSharedWindowAlternate:(id)sender {
    [STZWindow orderFrontSharedWindowWithAdvanceSettings:YES];
}

static NSRunningApplication *keyApplication(void) {
    return [[NSWorkspace sharedWorkspace] frontmostApplication] ?: [NSRunningApplication currentApplication];
}

static void toggleOptionsForKeyApplication(STZAppOptions flag) {
    NSRunningApplication *app = keyApplication();
    CFStringRef bundleID = (__bridge void *)[app bundleIdentifier];
    STZAppOptions options = STZGetAppOptionsForBundleIdentifier(bundleID);
    if (options & flag) {
        options &= ~flag;
    } else {
        options |= flag;
    }
    STZSetAppOptionsForBundleIdentifier(bundleID, options);
}

- (void)toggleEnabledForKeyApplication:(id)sender {
    toggleOptionsForKeyApplication(kSTZDisabledForApp);
}

- (void)toggleExcludingFlagsForKeyApplication:(id)sender {
    toggleOptionsForKeyApplication(kSTZFlagsExcludedForApp);
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem {
    if ([menuItem action] == @selector(toggleEnabledForKeyApplication:)) {
        NSRunningApplication *app = keyApplication();
        [menuItem setTitle:[NSString stringWithFormat:NSLocalizedString(@"enabled-for-app-name-%@", nil), [app localizedName]]];
        CFStringRef bundleID = (__bridge void *)[app bundleIdentifier];
        STZAppOptions options = STZGetAppOptionsForBundleIdentifier(bundleID);
        [menuItem setState:!(options & kSTZDisabledForApp)];
        return YES;
    }

    if ([menuItem action] == @selector(toggleExcludingFlagsForKeyApplication:)) {
        NSRunningApplication *app = keyApplication();
        CFStringRef bundleID = (__bridge void *)[app bundleIdentifier];
        STZAppOptions options = STZGetAppOptionsForBundleIdentifier(bundleID);
        [menuItem setEnabled:!(options & kSTZDisabledForApp)];
        [menuItem setState:!!(options & kSTZFlagsExcludedForApp)];
        return !(options & kSTZDisabledForApp);
    }

    return YES;
}

@end
