/*
 *  STZWindow.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZWindow.h"
#import <ServiceManagement/ServiceManagement.h>
#import "STZEventTap.h"
#import "STZControls.h"


@interface STZConfigViewController : NSViewController

- (void)noteWindowBecomeKey;

@end


@interface STZPermissionViewController : NSViewController

- (void)showPrompt:(nullable id)sender;

@end


/// A simple image view that has no intrinsic size.
@interface STZImageView : NSView

@property(nonatomic, nullable) NSImage *image;

@end


static NSString *trim(NSString *string) {
    return [string stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
}


static double clamp(double x, double const range[2]) {
    return x < range[0] ? range[0] : x > range[1] ? range[1] : x;
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

+ (void)orderFrontSharedWindowIfNeeded {
    [STZConfigViewController self];

    if (!STZSetEventTapEnabled(true)) {
        [self orderFrontSharedWindow];
    }
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

- (void)becomeKeyWindow {
    [super becomeKeyWindow];
    [(STZConfigViewController *)[self contentViewController] noteWindowBecomeKey];
}

@end


#define FIX_CHECKBOX_TRAILING  -5.0
#define INLINE_SPACING          7.0
#define SMALL_SPACING           10.0
#define NORMAL_SPACING          20.0
#define LARGE_SPACING           30.0


@implementation STZConfigViewController {
    NSButton           *_checkbox;
    STZModifierField   *_field;
    NSButton           *_zoomInRadio;
    NSButton           *_zoomOutRadio;
    NSSlider           *_speedSlider;
    NSSlider           *_inertiaSlider;
    uint8_t             _enableFailure;  //  0: none, 1: failed to disable, 2: failed to enable
}

static NSString *const STZScrollToZoomFlagsKey = @"STZScrollToZoomFlags";
static NSString *const STZScrollToZoomMagnifierKey = @"STZScrollToZoomMagnifier";
static NSString *const STZScrollMomentumToZoomAttenuationKey = @"STZScrollMomentumToZoomAttenuation";

static double const STZScrollToZoomMagnifierRange[] = {0.0005, 0.0105};
static double const STZScrollMomentumToZoomAttenuationRange[] = {0, 1};

+ (void)initialize {
    if (self != [STZConfigViewController class]) {return;}

    NSUserDefaults *ud = [NSUserDefaults standardUserDefaults];

    uint64_t flags = [STZModifierField validateModifiers:[ud integerForKey:STZScrollToZoomFlagsKey]];
    if (flags) {
        STZScrollToZoomFlags = flags;
    }

    if ([ud objectForKey:STZScrollToZoomMagnifierKey]) {
        double magnifier = [ud doubleForKey:STZScrollToZoomMagnifierKey];
        double signum = magnifier > 0 ? 1 : ((void)(magnifier = -magnifier), -1);
        STZScrollToZoomMagnifier = signum * clamp(magnifier, STZScrollToZoomMagnifierRange);
    }

    if ([ud objectForKey:STZScrollMomentumToZoomAttenuationKey]) {
        double attenuation = [ud doubleForKey:STZScrollMomentumToZoomAttenuationKey];
        STZScrollMomentumToZoomAttenuation = clamp(attenuation, STZScrollMomentumToZoomAttenuationRange);
    }
}

- (void)loadView {
    NSView *view = [[NSView alloc] init];
    [self setView:view];
    [self setTitle:NSLocalizedString(@"scroll-to-zoom", nil)];

    NSArray *checkboxTitles = [NSLocalizedString(@"enable-with-#", nil) componentsSeparatedByString:@"#"];

    _checkbox = [NSButton checkboxWithTitle:trim([checkboxTitles firstObject])
                                     target:self action:@selector(toggleScrollToZoom:)];
    NSTextField *checkboxTail = [checkboxTitles count] > 1
        ? [NSTextField labelWithString:trim([checkboxTitles objectAtIndex:1])] : nil;

    _field = [STZModifierField fieldWithModifiers:0
                                           target:self action:@selector(updateModifiers:)];

    NSTextField *directionLabel = [NSTextField labelWithString:NSLocalizedString(@"scroll-up-to", nil)];

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

    NSButton *launchAtLoginButton = [NSButton buttonWithTitle:NSLocalizedString(@"set-launch-at-login", nil)
                                                       target:self action:@selector(openLoginItems:)];

    [view setSubviews:@[_checkbox, _field, directionLabel, _zoomInRadio, _zoomOutRadio, speedLabel, _speedSlider, inertiaLabel, _inertiaSlider, launchAtLoginButton]];
    for (NSView *subviews in [view subviews]) {
        [subviews setTranslatesAutoresizingMaskIntoConstraints:NO];
    }

    [NSLayoutConstraint activateConstraints:@[
        [[_checkbox topAnchor] constraintEqualToAnchor:[view topAnchor] constant:NORMAL_SPACING],
        [[_checkbox leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:NORMAL_SPACING],

        [[_field firstBaselineAnchor] constraintEqualToAnchor:[_checkbox firstBaselineAnchor]],
        [[_field leadingAnchor] constraintEqualToAnchor:[_checkbox trailingAnchor] constant:INLINE_SPACING + FIX_CHECKBOX_TRAILING],

        [[directionLabel topAnchor] constraintEqualToAnchor:[_checkbox bottomAnchor] constant:SMALL_SPACING],
        [[directionLabel leadingAnchor] constraintEqualToAnchor:[_checkbox leadingAnchor] constant:NORMAL_SPACING],

        [[_zoomInRadio firstBaselineAnchor] constraintEqualToAnchor:[directionLabel firstBaselineAnchor]],
        [[_zoomInRadio leadingAnchor] constraintEqualToAnchor:[directionLabel trailingAnchor] constant:INLINE_SPACING],

        [[_zoomOutRadio firstBaselineAnchor] constraintEqualToAnchor:[_zoomInRadio firstBaselineAnchor]],
        [[_zoomOutRadio leadingAnchor] constraintEqualToAnchor:[_zoomInRadio trailingAnchor] constant:INLINE_SPACING + FIX_CHECKBOX_TRAILING],

        [[speedLabel topAnchor] constraintEqualToAnchor:[directionLabel bottomAnchor] constant:SMALL_SPACING],
        [[speedLabel leadingAnchor] constraintEqualToAnchor:[directionLabel leadingAnchor]],

        [[_speedSlider firstBaselineAnchor] constraintEqualToAnchor:[speedLabel firstBaselineAnchor]],
        [[_speedSlider leadingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor] constant:SMALL_SPACING],
        [[_speedSlider trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-LARGE_SPACING],

        [[inertiaLabel topAnchor] constraintEqualToAnchor:[speedLabel bottomAnchor] constant:SMALL_SPACING],
        [[inertiaLabel trailingAnchor] constraintEqualToAnchor:[speedLabel trailingAnchor]],

        [[_inertiaSlider firstBaselineAnchor] constraintEqualToAnchor:[inertiaLabel firstBaselineAnchor]],
        [[_inertiaSlider leadingAnchor] constraintEqualToAnchor:[inertiaLabel trailingAnchor] constant:SMALL_SPACING],
        [[_inertiaSlider trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-LARGE_SPACING],
        [[_inertiaSlider widthAnchor] constraintGreaterThanOrEqualToConstant:240],

        [[launchAtLoginButton trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-LARGE_SPACING],
        [[launchAtLoginButton topAnchor] constraintEqualToAnchor:[_inertiaSlider bottomAnchor] constant:SMALL_SPACING],
        [[launchAtLoginButton bottomAnchor] constraintEqualToAnchor:[view bottomAnchor] constant:-NORMAL_SPACING],
    ]];

    if (checkboxTail) {
        [view addSubview:checkboxTail];
        [checkboxTail setNextResponder:_checkbox];

        [checkboxTail setTranslatesAutoresizingMaskIntoConstraints:NO];

        [NSLayoutConstraint activateConstraints:@[
            [[checkboxTail firstBaselineAnchor] constraintEqualToAnchor:[_checkbox firstBaselineAnchor]],
            [[checkboxTail leadingAnchor] constraintEqualToAnchor:[_field trailingAnchor] constant:INLINE_SPACING],
        ]];
    }
}

- (void)viewWillAppear {
    [self reloadData];
}

- (void)dismissViewController:(NSViewController *)viewController {
    [self reloadData];
    [super dismissViewController:viewController];
}

- (void)reloadData {
    bool enabled = STZGetEventTapEnabled();
    [self setControlsEnabled:enabled];
    [_checkbox setState:enabled];
    [_field setModifiers:(NSEventModifierFlags)STZScrollToZoomFlags];
    [_zoomInRadio setState:STZScrollToZoomMagnifier > 0];
    [_zoomOutRadio setState:STZScrollToZoomMagnifier < 0];
    [_speedSlider setDoubleValue:fabs(STZScrollToZoomMagnifier)];
    [_inertiaSlider setDoubleValue:1 - STZScrollMomentumToZoomAttenuation];
}

- (void)setControlsEnabled:(BOOL)enabled {
    [_field setEnabled:enabled];
    [_zoomInRadio setEnabled:enabled];
    [_zoomOutRadio setEnabled:enabled];
    [_speedSlider setEnabled:enabled];
    [_inertiaSlider setEnabled:enabled];
}

- (void)noteWindowBecomeKey {
    if (!_enableFailure) {return;}

    BOOL enable = _enableFailure - 1;

    if (STZSetEventTapEnabled(enable)) {
        _enableFailure = 0;
        [self setControlsEnabled:enable];
        [_checkbox setState:enable];
        SMLoginItemSetEnabled(CFBundleGetIdentifier(CFBundleGetMainBundle()), enable);
    }
}

- (void)openLoginItems:(id)sender {
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.LoginItems-Settings.extension"]];
}

- (void)toggleScrollToZoom:(id)sender {
    _enableFailure = 0;
    BOOL enable = [_checkbox state] != NSOffState;

    if (STZSetEventTapEnabled(enable)) {
        [self setControlsEnabled:enable];
        SMLoginItemSetEnabled(CFBundleGetIdentifier(CFBundleGetMainBundle()), enable);

    } else {
        _enableFailure = enable + 1;
        [self presentViewController:[[STZPermissionViewController alloc] init]
            asPopoverRelativeToRect:[_checkbox bounds]
                             ofView:_checkbox
                      preferredEdge:NSRectEdgeMinY
                           behavior:NSPopoverBehaviorTransient];
    }
}

- (void)updateModifiers:(id)sender {
    STZScrollToZoomFlags = (CGEventFlags)[_field modifiers];
    [[NSUserDefaults standardUserDefaults] setInteger:STZScrollToZoomFlags 
                                               forKey:STZScrollToZoomFlagsKey];
}

- (void)updateSpeed:(id)sender {
    STZScrollToZoomMagnifier = ([_zoomOutRadio state] ? -1 : 1) * [_speedSlider doubleValue];
    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollToZoomMagnifier
                                              forKey:STZScrollToZoomMagnifierKey];
}

- (void)updateInertia:(id)sender {
    STZScrollMomentumToZoomAttenuation = 1 - [_inertiaSlider doubleValue];
    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollMomentumToZoomAttenuation
                                              forKey:STZScrollMomentumToZoomAttenuationKey];
}

@end


@implementation STZPermissionViewController

- (void)loadView {
    NSView *view = [[NSView alloc] init];
    [self setView:view];

    NSTextField *titleLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"permission-title", nil)];
    [titleLabel setFont:[NSFont boldSystemFontOfSize:0]];
    [titleLabel setSelectable:NO];

    NSTextField *messageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"permission-message", nil)];
    [messageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [messageLabel setSelectable:NO];

    STZImageView *warningImage = [[STZImageView alloc] init];
    [warningImage setImage:[NSImage imageNamed:NSImageNameCaution]];

    NSTextField *warningLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"permission-warning", nil)];
    [warningLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [warningLabel setSelectable:NO];

    NSButton *cancelButton = [NSButton buttonWithTitle:NSLocalizedString(@"cancel", nil)
                                                target:self action:@selector(dismissController:)];

    NSButton *openButton = [NSButton buttonWithTitle:NSLocalizedString(@"grant-permission", nil)
                                              target:self action:@selector(showPrompt:)];
    [openButton setKeyEquivalent:@"\r"];

    [view setSubviews:@[titleLabel, messageLabel, warningImage, warningLabel, openButton, cancelButton]];
    for (NSView *subviews in [view subviews]) {
        [subviews setTranslatesAutoresizingMaskIntoConstraints:NO];
    }

    [NSLayoutConstraint activateConstraints:@[
        [[titleLabel topAnchor] constraintEqualToAnchor:[view topAnchor] constant:NORMAL_SPACING],
        [[titleLabel leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:NORMAL_SPACING],
        [[titleLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-NORMAL_SPACING],

        [[messageLabel topAnchor] constraintEqualToAnchor:[titleLabel bottomAnchor] constant:NORMAL_SPACING],
        [[messageLabel leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:NORMAL_SPACING],
        [[messageLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-NORMAL_SPACING],

        [[warningImage topAnchor] constraintEqualToAnchor:[warningLabel topAnchor]],
        [[warningImage bottomAnchor] constraintEqualToAnchor:[warningLabel bottomAnchor]],
        [[warningImage leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:NORMAL_SPACING],
        [[warningImage widthAnchor] constraintEqualToAnchor:[warningImage heightAnchor]],

        [[warningLabel topAnchor] constraintEqualToAnchor:[messageLabel bottomAnchor] constant:SMALL_SPACING],
        [[warningLabel leadingAnchor] constraintEqualToAnchor:[warningImage trailingAnchor] constant:INLINE_SPACING],
        [[warningLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-NORMAL_SPACING],

        [[openButton topAnchor] constraintEqualToAnchor:[warningLabel bottomAnchor] constant:NORMAL_SPACING],
        [[openButton trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-NORMAL_SPACING],
        [[openButton widthAnchor] constraintGreaterThanOrEqualToConstant:60],
        [[openButton bottomAnchor] constraintEqualToAnchor:[view bottomAnchor] constant:-NORMAL_SPACING],

        [[cancelButton topAnchor] constraintEqualToAnchor:[openButton topAnchor]],
        [[cancelButton trailingAnchor] constraintEqualToAnchor:[openButton leadingAnchor] constant:-SMALL_SPACING],
        [[cancelButton widthAnchor] constraintGreaterThanOrEqualToConstant:60],

        [[titleLabel widthAnchor] constraintGreaterThanOrEqualToConstant:320],
    ]];
}

- (void)showPrompt:(id)sender {
    [self dismissController:nil];
    AXIsProcessTrustedWithOptions((__bridge void *)@{
        (__bridge id)kAXTrustedCheckOptionPrompt: (__bridge id)kCFBooleanTrue,
    });
}

@end


@implementation STZImageView

- (void)setImage:(NSImage *)image {
    if (image == _image) {return;}
    _image = image;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect {
    [_image drawInRect:[self bounds]];
}

@end
