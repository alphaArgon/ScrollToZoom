/*
 *  MGZWindow.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "MGZWindow.h"
#import "MGZScrollToZoom.h"
#import "STZOverlay.h"
#import "STZLaunchAtLogin.h"
#import "STZUIConstants.h"
#import "STZPermissionView.h"
#import "GeneratedAssetSymbols.h"


@interface MGZConfigViewController : NSViewController

@end


@implementation MGZWindow

+ (MGZWindow *)sharedWindow {
    static MGZWindow __weak *weakWindow = nil;
    if (weakWindow) {return weakWindow;}

    MGZWindow *window = [[MGZWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1)
                                                                 styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                                   backing:NSBackingStoreBuffered
                                                                     defer:YES];
    [window center];
    [window setReleasedWhenClosed:NO];
    weakWindow = window;
    return window;
}

+ (void)orderFrontSharedWindow {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [[self sharedWindow] makeKeyAndOrderFront:nil];
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag {
    self = [super initWithContentRect:contentRect styleMask:style backing:backingStoreType defer:flag];
    [self setContentViewController:[[MGZConfigViewController alloc] init]];
    [self setTitle:[[self contentViewController] title]];
    return self;
}

@end


@implementation MGZConfigViewController {
    NSButton           *_checkbox;
    NSSlider           *_speedSlider;
    NSSlider           *_inertiaSlider;
    NSButton           *_launchCheckbox;
    NSTimer            *_enableRetryTimer;
}

static double const STZScrollToZoomMagnifierRange[] = {0.0005, 0.0105};
static double const STZScrollMomentumToZoomAttenuationRange[] = {0, 1};

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    [self setTitle:NSLocalizedString(@"magic-zoom", nil)];
    return self;
}

- (void)loadView {
    NSView *view = [[NSView alloc] init];
    [self setView:view];

    _checkbox = [NSButton checkboxWithTitle:NSLocalizedString(@"enable-magic-zoom", nil)
                                     target:self action:@selector(toggleScrollToZoom:)];

    NSTextField *messageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"dot-dash-drag-to-zoom-message", nil)];
    [messageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [messageLabel setTextColor:[NSColor secondaryLabelColor]];
    [messageLabel setSelectable:NO];

    NSTextField *speedLabel = [NSTextField labelWithString:NSLocalizedString(@"speed", nil)];
    _speedSlider = [NSSlider sliderWithValue:0
                                    minValue:STZScrollToZoomMagnifierRange[0]
                                    maxValue:STZScrollToZoomMagnifierRange[1]
                                      target:self action:@selector(updateSpeed:)];
    [_speedSlider setNumberOfTickMarks:11];

    NSTextField *inertiaLabel = [NSTextField labelWithString:NSLocalizedString(@"inertia", nil)];
    _inertiaSlider = [NSSlider sliderWithValue:0
                                      minValue:STZScrollMomentumToZoomAttenuationRange[0]
                                      maxValue:STZScrollMomentumToZoomAttenuationRange[1]
                                        target:self action:@selector(updateInertia:)];
    [_inertiaSlider setNumberOfTickMarks:11];

    _launchCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"launch-at-login", nil)
                                           target:self action:@selector(toggleLaunchAtLogin:)];

    NSTextField *launchMessageLabel = nil;
    if (!STZShouldEnableLaunchAtLogin()) {
        launchMessageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"should-not-launch-at-login-message", nil)];
        [launchMessageLabel setFont:[NSFont toolTipsFontOfSize:0]];
        [launchMessageLabel setTextColor:[NSColor secondaryLabelColor]];
        [launchMessageLabel setSelectable:NO];
        [_launchCheckbox setEnabled:NO];
        [_launchCheckbox setTag:-1];
    } else {
        [_launchCheckbox setTag:0];
    }

    [view setSubviews:@[_checkbox, messageLabel, speedLabel, _speedSlider, inertiaLabel, _inertiaSlider, _launchCheckbox]];
    if (launchMessageLabel) {[view addSubview:launchMessageLabel];}
    for (NSView *subviews in [view subviews]) {
        [subviews setTranslatesAutoresizingMaskIntoConstraints:NO];
    }

    if (@available(macOS 26, *)) {
        [_speedSlider setControlSize:NSControlSizeSmall];
        [_inertiaSlider setControlSize:NSControlSizeSmall];
    }

    [NSLayoutConstraint activateConstraints:@[
        [[_checkbox topAnchor] constraintEqualToAnchor:[view topAnchor] constant:kSTZUINormalSpacing],
        [[_checkbox leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],

        [[messageLabel topAnchor] constraintEqualToAnchor:[_checkbox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[messageLabel leadingAnchor] constraintEqualToAnchor:[_checkbox leadingAnchor] constant:kSTZUICheckboxWidth],
        [[messageLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[speedLabel topAnchor] constraintEqualToAnchor:[messageLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[speedLabel leadingAnchor] constraintEqualToAnchor:[_checkbox leadingAnchor] constant:kSTZUICheckboxWidth],

        [[_speedSlider firstBaselineAnchor] constraintEqualToAnchor:[speedLabel firstBaselineAnchor] constant:kSTZUISliderBaselineOffset],
        [[_speedSlider leadingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor] constant:kSTZUISmallSpacing],
        [[_speedSlider trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[inertiaLabel topAnchor] constraintEqualToAnchor:[speedLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[inertiaLabel trailingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor]],

        [[_inertiaSlider firstBaselineAnchor] constraintEqualToAnchor:[inertiaLabel firstBaselineAnchor] constant:kSTZUISliderBaselineOffset],
        [[_inertiaSlider leadingAnchor] constraintEqualToAnchor:[inertiaLabel trailingAnchor] constant:kSTZUISmallSpacing],
        [[_inertiaSlider trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],
        [[_inertiaSlider widthAnchor] constraintGreaterThanOrEqualToConstant:240],

        [[_launchCheckbox topAnchor] constraintEqualToAnchor:[_inertiaSlider bottomAnchor] constant:kSTZUISmallSpacing],
        [[_launchCheckbox leadingAnchor] constraintEqualToAnchor:[_inertiaSlider leadingAnchor] constant:-kSTZUICheckboxWidth],
    ]];

    if (launchMessageLabel) {
        [NSLayoutConstraint activateConstraints:@[
            [[launchMessageLabel leadingAnchor] constraintEqualToAnchor:[_launchCheckbox leadingAnchor] constant:kSTZUICheckboxWidth],
            [[launchMessageLabel topAnchor] constraintEqualToAnchor:[_launchCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
            [[launchMessageLabel trailingAnchor] constraintEqualToAnchor:[_inertiaSlider trailingAnchor]],
            [[launchMessageLabel bottomAnchor] constraintEqualToAnchor:[view bottomAnchor] constant:-kSTZUINormalSpacing],
        ]];
    } else {
        [NSLayoutConstraint activateConstraints:@[
            [[_launchCheckbox bottomAnchor] constraintEqualToAnchor:[view bottomAnchor] constant:-kSTZUINormalSpacing],
        ]];
    }
}

- (void)viewWillAppear {
    [self reloadData];
}

- (void)reloadData {
    bool enabled = MGZGetScrollToZoomEnabled();
    [self setControlsEnabled:enabled];
    [_checkbox setState:enabled];
    [_speedSlider setDoubleValue:STZGetScrollToZoomMagnifier()];
    [_inertiaSlider setDoubleValue:1 - STZGetScrollMomentumToZoomAttenuation()];
    [_launchCheckbox setState:STZGetLaunchAtLoginEnabled()];
}

- (void)setControlsEnabled:(BOOL)enabled {
    [_speedSlider setEnabled:enabled];
    [_inertiaSlider setEnabled:enabled];
    [_launchCheckbox setEnabled:enabled && [_launchCheckbox tag] != -1];
}

- (void)toggleLaunchAtLogin:(id)sender {
    BOOL enable = [_launchCheckbox state] != NSOffState;
    STZSetLaunchAtLoginEnabled(enable);
}

- (void)toggleScrollToZoom:(id)sender {
    BOOL enable = [_checkbox state] != NSOffState;

    if (MGZSetScrollToZoomEnabled(enable)) {
        [self setControlsEnabled:enable];
        [_enableRetryTimer invalidate];
        _enableRetryTimer = nil;

    } else {
        [self presentViewController:[[STZPermissionViewController alloc] init]
            asPopoverRelativeToRect:[_checkbox bounds]
                             ofView:_checkbox
                      preferredEdge:NSRectEdgeMinY
                           behavior:NSPopoverBehaviorTransient];
    }
}

- (void)dismissViewController:(NSViewController *)viewController {
    [self reloadData];
    [super dismissViewController:viewController];

    if ([viewController isKindOfClass:[STZPermissionViewController self]]
     && ![(STZPermissionViewController *)viewController cancelled]
     && !_enableRetryTimer) {
        STZDebugLog("Begin checking permission");

        MGZConfigViewController __weak *weakSelf = self;
        _enableRetryTimer = [NSTimer scheduledTimerWithTimeInterval:1 repeats:YES block:^(NSTimer *timer) {
            MGZConfigViewController *this = weakSelf;
            if (!this) {
                [timer invalidate];
                STZDebugLog("Cancel checking permission");

            } else if (MGZSetScrollToZoomEnabled(true)) {
                [this setControlsEnabled:YES];
                [this->_checkbox setState:NSControlStateValueOn];
                [this->_enableRetryTimer invalidate];
                this->_enableRetryTimer = nil;
                STZDebugLog("End checking permission");
            }
        }];
    }
}

- (void)updateSpeed:(id)sender {
    STZSetScrollToZoomMagnifier([_speedSlider doubleValue]);
}

- (void)updateInertia:(id)sender {
    STZSetScrollMomentumToZoomAttenuation(1 - [_inertiaSlider doubleValue]);
}

@end
