/*
 *  STZPermissionView.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/7/29.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZPermissionView.h"
#import "STZUIConstants.h"


/// A simple image view that has no intrinsic size.
@interface STZImageView : NSView

@property(nonatomic, nullable) NSImage *image;

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
        [[titleLabel topAnchor] constraintEqualToAnchor:[view topAnchor] constant:kSTZUINormalSpacing],
        [[titleLabel leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],
        [[titleLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[messageLabel topAnchor] constraintEqualToAnchor:[titleLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[messageLabel leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],
        [[messageLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[warningImage topAnchor] constraintEqualToAnchor:[warningLabel topAnchor]],
        [[warningImage bottomAnchor] constraintEqualToAnchor:[warningLabel bottomAnchor]],
        [[warningImage leadingAnchor] constraintEqualToAnchor:[view leadingAnchor] constant:kSTZUINormalSpacing],
        [[warningImage widthAnchor] constraintEqualToAnchor:[warningImage heightAnchor]],

        [[warningLabel topAnchor] constraintEqualToAnchor:[messageLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[warningLabel leadingAnchor] constraintEqualToAnchor:[warningImage trailingAnchor] constant:kSTZUIInlineSpacing],
        [[warningLabel trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],

        [[openButton topAnchor] constraintEqualToAnchor:[warningLabel bottomAnchor] constant:kSTZUINormalSpacing],
        [[openButton trailingAnchor] constraintEqualToAnchor:[view trailingAnchor] constant:-kSTZUINormalSpacing],
        [[openButton widthAnchor] constraintGreaterThanOrEqualToConstant:60],
        [[openButton bottomAnchor] constraintEqualToAnchor:[view bottomAnchor] constant:-kSTZUINormalSpacing],

        [[cancelButton topAnchor] constraintEqualToAnchor:[openButton topAnchor]],
        [[cancelButton trailingAnchor] constraintEqualToAnchor:[openButton leadingAnchor] constant:-kSTZUISmallSpacing],
        [[cancelButton widthAnchor] constraintGreaterThanOrEqualToConstant:60],

        [[titleLabel widthAnchor] constraintGreaterThanOrEqualToConstant:320],
    ]];
}

- (void)viewWillAppear {
    _cancelled = YES;
    [super viewWillAppear];
}

- (void)showPrompt:(id)sender {
    _cancelled = NO;
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

