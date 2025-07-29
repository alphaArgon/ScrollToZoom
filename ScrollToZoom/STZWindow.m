/*
 *  STZWindow.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZWindow.h"
#import "STZPermissionView.h"
#import "STZScrollToZoom.h"
#import "STZSettings.h"
#import "STZLaunchAtLogin.h"
#import "STZControls.h"
#import "STZOptionsPanel.h"
#import "STZConsolePanel.h"
#import "STZUIConstants.h"
#import "GeneratedAssetSymbols.h"


@interface STZConfigViewController : NSViewController

- (void)setShowsConsoleButton:(BOOL)flag;

@end


static NSString *trim(NSString *string) {
    return [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}


@implementation STZWindow

+ (STZWindow *)sharedWindow {
    static STZWindow __weak *weakWindow = nil;
    if (weakWindow) {return weakWindow;}

    STZWindow *window = [[STZWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1)
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
    [self setContentViewController:[[STZConfigViewController alloc] init]];
    [self setTitle:[[self contentViewController] title]];
    return self;
}

- (void)orderWindow:(NSWindowOrderingMode)place relativeTo:(NSInteger)otherWin {
    [super orderWindow:place relativeTo:otherWin];
    BOOL showConsoleButton = !!([NSEvent modifierFlags] & NSEventModifierFlagOption);
    [(STZConfigViewController *)[self contentViewController] setShowsConsoleButton:showConsoleButton];
}

@end


@implementation STZConfigViewController {
    NSButton           *_checkbox;
    STZModifierField   *_field;
    NSButton           *_zoomInRadio;
    NSButton           *_zoomOutRadio;
    NSSlider           *_speedSlider;
    NSSlider           *_inertiaSlider;
    NSButton           *_dotDashCheckbox;
    NSButton           *_launchCheckbox;
    NSButton           *_optionsButton;
    NSButton           *_consoleButton;
    NSTimer            *_enableRetryTimer;
}

static double const STZScrollToZoomMagnifierRange[] = {0.0005, 0.0105};
static double const STZScrollMomentumToZoomAttenuationRange[] = {0, 1};

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    [self setTitle:NSLocalizedString(@"scroll-to-zoom", nil)];
    return self;
}

- (void)loadView {
    NSView *view = [[NSView alloc] init];
    [self setView:view];

    NSArray *checkboxTitles = [NSLocalizedString(@"enable-with-#", nil) componentsSeparatedByString:@"#"];

    _checkbox = [NSButton checkboxWithTitle:trim([checkboxTitles firstObject])
                                     target:self action:@selector(toggleScrollToZoom:)];
    NSTextField *checkboxTail = [checkboxTitles count] > 1
        ? [NSTextField labelWithString:trim([checkboxTitles objectAtIndex:1])] : nil;

    _field = [STZModifierField fieldWithModifiers:0
                                           target:self action:@selector(updateFlags:)];

    NSTextField *directionLabel = [NSTextField labelWithString:NSLocalizedString(@"swipe-up-to", nil)];

    _zoomInRadio = [NSButton radioButtonWithTitle:NSLocalizedString(@"zoom-in", nil)
                                           target:self action:@selector(updateSpeed:)];
    _zoomOutRadio = [NSButton radioButtonWithTitle:NSLocalizedString(@"zoom-out", nil)
                                            target:self action:@selector(updateSpeed:)];

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

    NSTextField *inertiaMessageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"inertia-message", nil)];
    [inertiaMessageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [inertiaMessageLabel setTextColor:[NSColor secondaryLabelColor]];
    [inertiaMessageLabel setSelectable:NO];

    _dotDashCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"dot-dash-drag-to-zoom", nil)
                                            target:self action:@selector(toggleDotDashDrag:)];

    NSTextField *dotDashMessageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"dot-dash-drag-to-zoom-message", nil)];
    [dotDashMessageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [dotDashMessageLabel setTextColor:[NSColor secondaryLabelColor]];
    [dotDashMessageLabel setSelectable:NO];

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

    _optionsButton = [NSButton buttonWithTitle:NSLocalizedString(@"open-options-for-apps", nil)
                                        target:self action:@selector(openOptionsForApps:)];

    _consoleButton = [NSButton buttonWithImage:[NSImage imageNamed:ACImageNameDebug]
                                        target:self action:@selector(openConsole:)];
    [_consoleButton setBezelStyle:NSBezelStyleCircular];
    [_consoleButton setToolTip:NSLocalizedString(@"open-console", nil)];

    [view setSubviews:@[_checkbox, _field, directionLabel, _zoomInRadio, _zoomOutRadio,
                        speedLabel, _speedSlider, inertiaLabel, _inertiaSlider, inertiaMessageLabel,
                        _dotDashCheckbox, dotDashMessageLabel,
                        _launchCheckbox, _optionsButton, _consoleButton]];
    if (checkboxTail) {[view addSubview:checkboxTail];}
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

        [[_field firstBaselineAnchor] constraintEqualToAnchor:[_checkbox firstBaselineAnchor]],
        [[_field leadingAnchor] constraintEqualToAnchor:[_checkbox trailingAnchor] constant:kSTZUIInlineSpacing + kSTZUIFixCheckboxTrailing],

        [[directionLabel topAnchor] constraintEqualToAnchor:[_checkbox bottomAnchor] constant:kSTZUISmallSpacing],
        [[directionLabel leadingAnchor] constraintEqualToAnchor:[_checkbox leadingAnchor] constant:kSTZUICheckboxWidth],

        [[_zoomInRadio firstBaselineAnchor] constraintEqualToAnchor:[directionLabel firstBaselineAnchor]],
        [[_zoomInRadio leadingAnchor] constraintEqualToAnchor:[directionLabel trailingAnchor] constant:kSTZUIInlineSpacing],

        [[_zoomOutRadio firstBaselineAnchor] constraintEqualToAnchor:[_zoomInRadio firstBaselineAnchor]],
        [[_zoomOutRadio leadingAnchor] constraintEqualToAnchor:[_zoomInRadio trailingAnchor] constant:kSTZUIInlineSpacing + kSTZUIFixCheckboxTrailing],

        [[speedLabel topAnchor] constraintEqualToAnchor:[directionLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[speedLabel leadingAnchor] constraintEqualToAnchor:[directionLabel leadingAnchor]],

        [[_speedSlider firstBaselineAnchor] constraintEqualToAnchor:[speedLabel firstBaselineAnchor] constant:kSTZUISliderBaselineOffset],
        [[_speedSlider leadingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor] constant:kSTZUISmallSpacing],
        [[_speedSlider trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[inertiaLabel topAnchor] constraintEqualToAnchor:[speedLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[inertiaLabel trailingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor]],

        [[_inertiaSlider firstBaselineAnchor] constraintEqualToAnchor:[inertiaLabel firstBaselineAnchor] constant:kSTZUISliderBaselineOffset],
        [[_inertiaSlider leadingAnchor] constraintEqualToAnchor:[inertiaLabel trailingAnchor] constant:kSTZUISmallSpacing],
        [[_inertiaSlider trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],
        [[_inertiaSlider widthAnchor] constraintGreaterThanOrEqualToConstant:240],

        [[inertiaMessageLabel topAnchor] constraintEqualToAnchor:[inertiaLabel bottomAnchor] constant:kSTZUIInlineSpacing - kSTZUISliderBaselineOffset],
        [[inertiaMessageLabel leadingAnchor] constraintEqualToAnchor:[_inertiaSlider leadingAnchor]],
        [[inertiaMessageLabel trailingAnchor] constraintEqualToAnchor:[_inertiaSlider trailingAnchor]],

        [[_dotDashCheckbox leadingAnchor] constraintEqualToAnchor:[_inertiaSlider leadingAnchor] constant:-kSTZUICheckboxWidth],
        [[_dotDashCheckbox topAnchor] constraintEqualToAnchor:[inertiaMessageLabel bottomAnchor] constant:kSTZUISmallSpacing],

        [[dotDashMessageLabel leadingAnchor] constraintEqualToAnchor:[_inertiaSlider leadingAnchor]],
        [[dotDashMessageLabel topAnchor] constraintEqualToAnchor:[_dotDashCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[dotDashMessageLabel trailingAnchor] constraintEqualToAnchor:[_inertiaSlider trailingAnchor]],

        [[_launchCheckbox leadingAnchor] constraintEqualToAnchor:[_inertiaSlider leadingAnchor] constant:-kSTZUICheckboxWidth],
        [[_launchCheckbox topAnchor] constraintEqualToAnchor:[dotDashMessageLabel bottomAnchor] constant:kSTZUISmallSpacing],

        [[_optionsButton trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],
        [[_optionsButton topAnchor] constraintEqualToAnchor:[(launchMessageLabel ?: _launchCheckbox) bottomAnchor] constant:kSTZUINormalSpacing - kSTZUIInlineSpacing],
        [[_optionsButton bottomAnchor] constraintEqualToAnchor:[view bottomAnchor] constant:-kSTZUINormalSpacing],

        [[_consoleButton leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],
        [[_consoleButton centerYAnchor] constraintEqualToAnchor:[_optionsButton centerYAnchor]],
    ]];

    if (checkboxTail) {
        [NSLayoutConstraint activateConstraints:@[
            [[checkboxTail firstBaselineAnchor] constraintEqualToAnchor:[_checkbox firstBaselineAnchor]],
            [[checkboxTail leadingAnchor] constraintEqualToAnchor:[_field trailingAnchor] constant:kSTZUIInlineSpacing],
        ]];
    }

    if (launchMessageLabel) {
        [NSLayoutConstraint activateConstraints:@[
            [[launchMessageLabel leadingAnchor] constraintEqualToAnchor:[_inertiaSlider leadingAnchor]],
            [[launchMessageLabel topAnchor] constraintEqualToAnchor:[_launchCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
            [[launchMessageLabel trailingAnchor] constraintEqualToAnchor:[_inertiaSlider trailingAnchor]],
        ]];
    }
}

- (void)setShowsConsoleButton:(BOOL)flag {
    [_consoleButton setHidden:!flag];
}

- (void)viewWillAppear {
    [self reloadData];
}

- (void)reloadData {
    bool enabled = STZGetScrollToZoomEnabled();
    [self setControlsEnabled:enabled];
    [_checkbox setState:enabled];
    [_field setFlags:STZGetScrollToZoomFlags()];
    [_zoomInRadio setState:STZGetScrollToZoomMagnifier() > 0];
    [_zoomOutRadio setState:STZGetScrollToZoomMagnifier() < 0];
    [_speedSlider setDoubleValue:fabs(STZGetScrollToZoomMagnifier())];
    [_inertiaSlider setDoubleValue:1 - STZGetScrollMomentumToZoomAttenuation()];
    [_dotDashCheckbox setState:STZGetDotDashDragToZoomEnabled()];
    [_launchCheckbox setState:STZGetLaunchAtLoginEnabled()];
}

- (void)setControlsEnabled:(BOOL)enabled {
    [_field setEnabled:enabled];
    [_zoomInRadio setEnabled:enabled];
    [_zoomOutRadio setEnabled:enabled];
    [_speedSlider setEnabled:enabled];
    [_inertiaSlider setEnabled:enabled];
    [_optionsButton setEnabled:enabled];
    [_dotDashCheckbox setEnabled:enabled];
    [_launchCheckbox setEnabled:enabled && [_launchCheckbox tag] != -1];
}

- (void)openOptionsForApps:(id)sender {
    [STZOptionsPanel orderFrontSharedPanel];
}

- (void)openConsole:(id)sender {
    [STZConsolePanel orderFrontSharedPanel];
}

- (void)toggleLaunchAtLogin:(id)sender {
    BOOL enable = [_launchCheckbox state] != NSOffState;
    STZSetLaunchAtLoginEnabled(enable);
}

- (void)toggleDotDashDrag:(id)sender {
    BOOL enable = [_dotDashCheckbox state] != NSOffState;
    STZSetDotDashDragToZoomEnabled(enable);
}

- (void)toggleScrollToZoom:(id)sender {
    BOOL enable = [_checkbox state] != NSOffState;

    if (STZSetScrollToZoomEnabled(enable)) {
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

        STZConfigViewController __weak *weakSelf = self;
        _enableRetryTimer = [NSTimer scheduledTimerWithTimeInterval:1 repeats:YES block:^(NSTimer *timer) {
            STZConfigViewController *this = weakSelf;
            if (!this) {
                [timer invalidate];
                STZDebugLog("Cancel checking permission");

            } else if (STZSetScrollToZoomEnabled(true)) {
                [this setControlsEnabled:YES];
                [this->_checkbox setState:NSControlStateValueOn];
                [this->_enableRetryTimer invalidate];
                this->_enableRetryTimer = nil;
                STZDebugLog("End checking permission");
            }
        }];
    }
}

- (void)updateFlags:(id)sender {
    STZSetScrollToZoomFlags([_field flags]);
}

- (void)updateSpeed:(id)sender {
    STZSetScrollToZoomMagnifier(([_zoomOutRadio state] ? -1 : 1) * [_speedSlider doubleValue]);
}

- (void)updateInertia:(id)sender {
    STZSetScrollMomentumToZoomAttenuation(1 - [_inertiaSlider doubleValue]);
}

@end
