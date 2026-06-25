/*
 *  STZWindow.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZWindow.h"
#import "STZPermissionView.h"
#import "STZEventHandling.h"
#import "STZSettings.h"
#import "STZLaunchAtLogin.h"
#import "STZControls.h"
#import "STZOptionsPanel.h"
#import "STZConsolePanel.h"
#import "STZUIConstants.h"
#import "GeneratedAssetSymbols.h"


@interface STZConfigViewController : NSViewController

@property(nonatomic) BOOL showsAdvancedSettings;

@end


static NSString *trim(NSString *string) {
    return [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}


@implementation STZWindow

+ (STZWindow *)sharedWindow {
    static STZWindow __weak *weakWindow = nil;
    if (weakWindow) {return weakWindow;}

    STZWindow *window = [[STZWindow alloc] initWithContentRect:NSMakeRect(0, 0, 200, 200)
                                                                 styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                                   backing:NSBackingStoreBuffered
                                                                     defer:YES];
    [window center];
    [window setReleasedWhenClosed:NO];
    weakWindow = window;
    return window;
}

+ (void)orderFrontSharedWindowWithAdvanceSettings:(BOOL)advanced {
    STZWindow *sharedWindow = [self sharedWindow];

    STZConfigViewController *viewController = (STZConfigViewController *)[sharedWindow contentViewController];
    if ([viewController showsAdvancedSettings] != advanced) {
        [viewController setShowsAdvancedSettings:advanced];
        if (!advanced) {
            NSRect frame = [sharedWindow frame];
            [sharedWindow setFrame:NSMakeRect(NSMinX(frame), NSMaxY(frame) - 200, NSWidth(frame), 200) display:NO];
        }
    }

    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [sharedWindow makeKeyAndOrderFront:nil];
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

@end


@implementation STZConfigViewController {
    NSButton           *_triggerFlagsCheckbox;
    STZModifierField   *_triggerFlagsField;
    NSButton           *_magicZoomCheckbox;
    NSButton           *_zoomInRadio;
    NSButton           *_zoomOutRadio;
    NSSlider           *_speedSlider;
    NSSlider           *_inertiaSlider;
    NSButton           *_launchCheckbox;
    NSButton           *_dictatorshipCheckbox;
    NSTextField        *_dictatorshipMessageLabel;
    NSButton           *_optionsButton;
    NSButton           *_consoleButton;
    NSLayoutConstraint *_optionsBelowLaunchConstraint;
    NSLayoutConstraint *_optionsBelowDictatorshipConstraint;
    NSTimer            *_enableRetryTimer;
    STZModes            _pendingModes;
}

static double const STZMagnificationScalarRange[] = {0.0005, 0.0105};
static double const STZMomentumZoomAttenuationRange[] = {0, 1};

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    [self setTitle:NSLocalizedString(@"scroll-to-zoom", nil)];
    return self;
}

- (void)loadView {
    NSView *view = [[NSView alloc] init];
    [self setView:view];

    NSArray *checkboxTitles = [NSLocalizedString(@"enable-with-key-or-button-#", nil) componentsSeparatedByString:@"#"];

    _triggerFlagsCheckbox = [NSButton checkboxWithTitle:trim([checkboxTitles firstObject])
                                     target:self action:@selector(toggleTriggerFlags:)];
    NSTextField *checkboxTail = [checkboxTitles count] > 1
        ? [NSTextField labelWithString:trim([checkboxTitles objectAtIndex:1])] : nil;

    _triggerFlagsField = [STZModifierField fieldWithModifiers:0
                                           target:self action:@selector(updateFlags:)];

    _magicZoomCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"enable-magic-zoom", nil)
                                            target:self action:@selector(toggleMagicZoom:)];

    NSTextField *magicZoomMessageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"magic-zoom-message", nil)];
    [magicZoomMessageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [magicZoomMessageLabel setTextColor:[NSColor secondaryLabelColor]];
    [magicZoomMessageLabel setSelectable:NO];

    NSBox *box = [[NSBox alloc] init];
    [box setTitlePosition:NSNoTitle];

    NSTextField *directionLabel = [NSTextField labelWithString:NSLocalizedString(@"swipe-up-to", nil)];

    _zoomInRadio = [NSButton radioButtonWithTitle:NSLocalizedString(@"-zoom-in", nil)
                                           target:self action:@selector(updateSpeed:)];
    _zoomOutRadio = [NSButton radioButtonWithTitle:NSLocalizedString(@"-zoom-out", nil)
                                            target:self action:@selector(updateSpeed:)];

    NSTextField *speedLabel = [NSTextField labelWithString:NSLocalizedString(@"speed:", nil)];
    _speedSlider = [NSSlider sliderWithValue:0
                                    minValue:STZMagnificationScalarRange[0]
                                    maxValue:STZMagnificationScalarRange[1]
                                      target:self action:@selector(updateSpeed:)];
    [_speedSlider setNumberOfTickMarks:11];

    NSTextField *inertiaLabel = [NSTextField labelWithString:NSLocalizedString(@"inertia:", nil)];
    _inertiaSlider = [NSSlider sliderWithValue:0
                                      minValue:STZMomentumZoomAttenuationRange[0]
                                      maxValue:STZMomentumZoomAttenuationRange[1]
                                        target:self action:@selector(updateInertia:)];
    [_inertiaSlider setNumberOfTickMarks:11];

    NSTextField *inertiaMessageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"inertia-message", nil)];
    [inertiaMessageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [inertiaMessageLabel setTextColor:[NSColor secondaryLabelColor]];
    [inertiaMessageLabel setSelectable:NO];

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

    _dictatorshipCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"wants-dictatorship", nil)
                                                 target:self action:@selector(toggleDictatorship:)];

    _dictatorshipMessageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"wants-dictatorship-message", nil)];
    [_dictatorshipMessageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [_dictatorshipMessageLabel setTextColor:[NSColor secondaryLabelColor]];
    [_dictatorshipMessageLabel setSelectable:NO];

    _optionsButton = [NSButton buttonWithTitle:NSLocalizedString(@"open-options-for-apps", nil)
                                        target:self action:@selector(openOptionsForApps:)];

    _consoleButton = [NSButton buttonWithImage:[NSImage imageNamed:ACImageNameDebug]
                                        target:self action:@selector(openConsole:)];
    [_consoleButton setBezelStyle:NSBezelStyleCircular];
    [_consoleButton setToolTip:NSLocalizedString(@"open-console", nil)];

    [view setSubviews:@[_triggerFlagsCheckbox, _triggerFlagsField,
                        _magicZoomCheckbox, magicZoomMessageLabel,
                        box, _launchCheckbox, _dictatorshipCheckbox, _dictatorshipMessageLabel,
                        _optionsButton, _consoleButton]];
    [[box contentView] setSubviews:@[directionLabel, _zoomInRadio, _zoomOutRadio,
                                     speedLabel, _speedSlider,
                                     inertiaLabel, _inertiaSlider, inertiaMessageLabel]];

    if (checkboxTail) {[view addSubview:checkboxTail];}
    if (launchMessageLabel) {[view addSubview:launchMessageLabel];}

    for (NSView *subviews in [view subviews]) {
        [subviews setTranslatesAutoresizingMaskIntoConstraints:NO];
    }
    for (NSView *subviews in [[box contentView] subviews]) {
        [subviews setTranslatesAutoresizingMaskIntoConstraints:NO];
    }

    if (@available(macOS 26, *)) {
        [_speedSlider setControlSize:NSControlSizeSmall];
        [_inertiaSlider setControlSize:NSControlSizeSmall];
    }

    _optionsBelowLaunchConstraint = [[_optionsButton topAnchor] constraintEqualToAnchor:[(launchMessageLabel ?: _launchCheckbox) bottomAnchor] constant:kSTZUINormalSpacing - kSTZUIInlineSpacing];
    _optionsBelowDictatorshipConstraint = [[_optionsButton topAnchor] constraintEqualToAnchor:[_dictatorshipMessageLabel bottomAnchor] constant:kSTZUINormalSpacing - kSTZUIInlineSpacing];

    if ([[_triggerFlagsCheckbox title] length]) {
        [[[_triggerFlagsField firstBaselineAnchor] constraintEqualToAnchor:[_triggerFlagsCheckbox firstBaselineAnchor]] setActive:YES];
    } else {
        [[[_triggerFlagsField centerYAnchor] constraintEqualToAnchor:[_triggerFlagsCheckbox centerYAnchor]] setActive:YES];
    }

    [NSLayoutConstraint activateConstraints:@[
        [[_triggerFlagsCheckbox topAnchor] constraintEqualToAnchor:[view topAnchor] constant:kSTZUINormalSpacing],
        [[_triggerFlagsCheckbox leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],

        [[_triggerFlagsField leadingAnchor] constraintEqualToAnchor:[_triggerFlagsCheckbox trailingAnchor] constant:kSTZUIInlineSpacing + kSTZUIFixCheckboxTrailing],

        [[_magicZoomCheckbox topAnchor] constraintEqualToAnchor:[_triggerFlagsCheckbox bottomAnchor] constant:kSTZUISmallSpacing],
        [[_magicZoomCheckbox leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],

        [[magicZoomMessageLabel topAnchor] constraintEqualToAnchor:[_magicZoomCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[magicZoomMessageLabel leadingAnchor] constraintEqualToAnchor:[_magicZoomCheckbox leadingAnchor] constant:kSTZUICheckboxWidth],
        [[magicZoomMessageLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[box topAnchor] constraintEqualToAnchor:[magicZoomMessageLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[box leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],
        [[box trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[directionLabel topAnchor] constraintEqualToAnchor:[box topAnchor] constant:kSTZUISmallSpacing],
        [[directionLabel leadingAnchor] constraintEqualToAnchor:[box leadingAnchor] constant:kSTZUICheckboxWidth],

        [[_zoomInRadio firstBaselineAnchor] constraintEqualToAnchor:[directionLabel firstBaselineAnchor]],
        [[_zoomInRadio leadingAnchor] constraintEqualToAnchor:[directionLabel trailingAnchor] constant:kSTZUIInlineSpacing],

        [[_zoomOutRadio firstBaselineAnchor] constraintEqualToAnchor:[_zoomInRadio firstBaselineAnchor]],
        [[_zoomOutRadio leadingAnchor] constraintEqualToAnchor:[_zoomInRadio trailingAnchor] constant:kSTZUIInlineSpacing + kSTZUIFixCheckboxTrailing],
        [[_zoomOutRadio trailingAnchor] constraintLessThanOrEqualToAnchor:[box trailingAnchor] constant:-kSTZUISmallSpacing],

        [[speedLabel topAnchor] constraintEqualToAnchor:[directionLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[speedLabel leadingAnchor] constraintEqualToAnchor:[directionLabel leadingAnchor]],

        [[_speedSlider firstBaselineAnchor] constraintEqualToAnchor:[speedLabel firstBaselineAnchor] constant:kSTZUISliderBaselineOffset],
        [[_speedSlider leadingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor] constant:kSTZUISmallSpacing],
        [[_speedSlider trailingAnchor] constraintEqualToAnchor:[box trailingAnchor] constant:-kSTZUINormalSpacing],

        [[inertiaLabel topAnchor] constraintEqualToAnchor:[speedLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[inertiaLabel trailingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor]],

        [[_inertiaSlider firstBaselineAnchor] constraintEqualToAnchor:[inertiaLabel firstBaselineAnchor] constant:kSTZUISliderBaselineOffset],
        [[_inertiaSlider leadingAnchor] constraintEqualToAnchor:[inertiaLabel trailingAnchor] constant:kSTZUISmallSpacing],
        [[_inertiaSlider trailingAnchor] constraintEqualToAnchor:[box trailingAnchor] constant:-kSTZUINormalSpacing],
        [[_inertiaSlider widthAnchor] constraintGreaterThanOrEqualToConstant:240],

        [[inertiaMessageLabel topAnchor] constraintEqualToAnchor:[inertiaLabel bottomAnchor] constant:kSTZUIInlineSpacing - kSTZUISliderBaselineOffset],
        [[inertiaMessageLabel leadingAnchor] constraintEqualToAnchor:[_inertiaSlider leadingAnchor]],
        [[inertiaMessageLabel trailingAnchor] constraintEqualToAnchor:[_inertiaSlider trailingAnchor]],
        [[inertiaMessageLabel bottomAnchor] constraintEqualToAnchor:[box bottomAnchor] constant:-kSTZUISmallSpacing],

        [[_launchCheckbox topAnchor] constraintEqualToAnchor:[box bottomAnchor] constant:kSTZUISmallSpacing],
        [[_launchCheckbox leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],

        [[_dictatorshipCheckbox topAnchor] constraintEqualToAnchor:[(launchMessageLabel ?: _launchCheckbox) bottomAnchor] constant:kSTZUINormalSpacing - kSTZUIInlineSpacing],
        [[_dictatorshipCheckbox leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],

        [[_dictatorshipMessageLabel topAnchor] constraintEqualToAnchor:[_dictatorshipCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[_dictatorshipMessageLabel leadingAnchor] constraintEqualToAnchor:[_dictatorshipCheckbox leadingAnchor] constant:kSTZUICheckboxWidth],
        [[_dictatorshipMessageLabel trailingAnchor] constraintEqualToAnchor:[_inertiaSlider trailingAnchor]],

        [[_optionsButton trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],
        [[_optionsButton bottomAnchor] constraintEqualToAnchor:[view bottomAnchor] constant:-kSTZUINormalSpacing],

        [[_consoleButton leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],
        [[_consoleButton centerYAnchor] constraintEqualToAnchor:[_optionsButton centerYAnchor]],
    ]];

    if (checkboxTail) {
        [NSLayoutConstraint activateConstraints:@[
            [[checkboxTail firstBaselineAnchor] constraintEqualToAnchor:[_triggerFlagsField firstBaselineAnchor]],
            [[checkboxTail leadingAnchor] constraintEqualToAnchor:[_triggerFlagsField trailingAnchor] constant:kSTZUIInlineSpacing],
        ]];
    }

    if (launchMessageLabel) {
        [NSLayoutConstraint activateConstraints:@[
            [[launchMessageLabel leadingAnchor] constraintEqualToAnchor:[_launchCheckbox leadingAnchor] constant:kSTZUICheckboxWidth],
            [[launchMessageLabel topAnchor] constraintEqualToAnchor:[_launchCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
            [[launchMessageLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],
        ]];
    }

    [self updateAdvancedSettingsVisibility];
}

- (void)setShowsAdvancedSettings:(BOOL)flag {
    _showsAdvancedSettings = flag;
    [self updateAdvancedSettingsVisibility];
}

- (void)updateAdvancedSettingsVisibility {
    [_consoleButton setHidden:!_showsAdvancedSettings];
    [_dictatorshipCheckbox setHidden:!_showsAdvancedSettings];

    if (_showsAdvancedSettings) {
        [_dictatorshipMessageLabel setHidden:NO];
        [_optionsBelowLaunchConstraint setActive:NO];
        [_optionsBelowDictatorshipConstraint setActive:YES];
    } else {
        [_optionsBelowDictatorshipConstraint setActive:NO];
        [_optionsBelowLaunchConstraint setActive:YES];
        [_dictatorshipMessageLabel setHidden:YES];
    }
}

- (void)viewWillAppear {
    [self reloadData];
}

- (void)reloadData {
    STZModes modes = STZGetWorkingModes();
    [self setControlsEnabledForModes:modes];
    [_triggerFlagsCheckbox setState:(modes & kSTZTriggerFlagsEnabled) != 0];
    [_triggerFlagsField setFlags:STZGetTriggerFlags()];
    [_zoomInRadio setState:STZGetMagnificationScalar() > 0];
    [_zoomOutRadio setState:STZGetMagnificationScalar() < 0];
    [_speedSlider setDoubleValue:fabs(STZGetMagnificationScalar())];
    [_inertiaSlider setDoubleValue:1 - STZGetMomentumZoomAttenuation()];
    [_magicZoomCheckbox setState:(modes & kSTZMagicZoomEnabled) != 0];
    [_dictatorshipCheckbox setState:(modes & kSTZWantsDictatorship) != 0];
    [_launchCheckbox setState:STZGetLaunchAtLoginEnabled()];
}

- (void)setControlsEnabledForModes:(STZModes)modes {
    BOOL triggerFlagsEnabled = (modes & kSTZTriggerFlagsEnabled) != 0;
    BOOL magicZoomEnabled = (modes & kSTZMagicZoomEnabled) != 0;
    BOOL zoomSettingsEnabled = triggerFlagsEnabled || magicZoomEnabled;

    [_triggerFlagsField setEnabled:triggerFlagsEnabled];
    [_zoomInRadio setEnabled:zoomSettingsEnabled];
    [_zoomOutRadio setEnabled:zoomSettingsEnabled];
    [_speedSlider setEnabled:zoomSettingsEnabled];
    [_inertiaSlider setEnabled:zoomSettingsEnabled];
    [_optionsButton setEnabled:triggerFlagsEnabled];
    [_launchCheckbox setEnabled:[_launchCheckbox tag] != -1];
    [_dictatorshipCheckbox setEnabled:zoomSettingsEnabled];
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

- (void)toggleTriggerFlags:(id)sender {
    [self toggleMode:kSTZTriggerFlagsEnabled byCheckbox:_triggerFlagsCheckbox];
}

- (void)toggleMagicZoom:(id)sender {
    [self toggleMode:kSTZMagicZoomEnabled byCheckbox:_magicZoomCheckbox];
}

- (void)toggleDictatorship:(id)sender {
    [self toggleMode:kSTZWantsDictatorship byCheckbox:_dictatorshipCheckbox];
}

- (void)toggleMode:(STZModes)mode byCheckbox:(NSButton *)checkbox {
    STZModes modes = STZGetPreferredModes();
    if ([checkbox state]) {
        modes |= mode;
    } else {
        modes &= ~mode;
    }

    if ([self commitModes:modes presentingFromView:checkbox]) {
        [_enableRetryTimer invalidate];
        _enableRetryTimer = nil;
    }
}

- (BOOL)commitModes:(STZModes)modes presentingFromView:(NSView *)view {
    BOOL committed = STZSetWorkingModes(modes);
    if (committed || !(modes & kSTZPracticalModesMask)) {
        STZSetPreferredModes(modes);
        [self reloadData];
        return YES;
    }

    _pendingModes = modes;
    [self reloadData];

    NSRect anchorRect = [view bounds];
    NSView *checkView = [[view subviews] firstObject];
    if (checkView) {
        NSRect checkFrame = [checkView frame];
        if (checkFrame.size.width == checkFrame.size.height) {
            anchorRect = checkFrame;
        }
    }

    [self presentViewController:[[STZPermissionViewController alloc] init]
        asPopoverRelativeToRect:anchorRect
                         ofView:view
                  preferredEdge:NSRectEdgeMinY
                       behavior:NSPopoverBehaviorTransient];
    return NO;
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

            } else if (STZSetWorkingModes(this->_pendingModes)) {
                STZSetPreferredModes(this->_pendingModes);
                [this reloadData];
                [this->_enableRetryTimer invalidate];
                this->_enableRetryTimer = nil;
                STZDebugLog("End checking permission");
            }
        }];
    }
}

- (void)updateFlags:(id)sender {
    STZSetTriggerFlags([_triggerFlagsField flags]);
}

- (void)updateSpeed:(id)sender {
    STZSetMagnificationScalar(([_zoomOutRadio state] ? -1 : 1) * [_speedSlider doubleValue]);
}

- (void)updateInertia:(id)sender {
    STZSetMomentumZoomAttenuation(1 - [_inertiaSlider doubleValue]);
}

@end
