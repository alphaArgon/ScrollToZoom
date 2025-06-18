/*
 *  STZControls.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZControls.h"
#import "GeneratedAssetSymbols.h"


#define TAHOE_CONTROL_RADIUS ((CGFloat)5.0)


NSFont *STZSymbolsFontOfSize(CGFloat size) {
    static CTFontDescriptorRef desc = NULL;
    if (!desc) {
        NSURL *fontURL = [[NSBundle mainBundle] URLForResource:@"STZSymbols" withExtension:@".ttf"];
        CFArrayRef descs = CTFontManagerCreateFontDescriptorsFromURL((__bridge void *)fontURL);
        CTFontDescriptorRef desc0 = CFArrayGetValueAtIndex(descs, 0);

        CTFontRef systemFont = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 0, nil);
        CTFontDescriptorRef systemDesc = CTFontCopyFontDescriptor(systemFont);

        desc = CTFontDescriptorCreateCopyWithAttributes(systemDesc, (__bridge void *)@{
            (__bridge id)kCTFontCascadeListAttribute: @[(__bridge id)desc0]
        });

        CFRelease(systemDesc);
        CFRelease(systemFont);
        CFRelease(descs);
    }

    return [NSFont fontWithDescriptor:(__bridge id)desc size:size];
}


@implementation STZModifierField {
    BOOL                    _editing;
    BOOL                    _highlighted;
    STZFlags                _accumulatedFlags;
    NSTextField            *_symbolLabel;
    CGFloat                 _symbolMaxWidth;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];

    CFStringRef description;
    STZValidateFlags(kSTZModifiersMask, &description);
    _symbolLabel = [NSTextField labelWithString:(__bridge id)description];
    [_symbolLabel setRefusesFirstResponder:YES];
    [_symbolLabel setEditable:NO];
    [_symbolLabel setSelectable:NO];
    [_symbolLabel setFont:STZSymbolsFontOfSize(0)];

    _symbolMaxWidth = [_symbolLabel intrinsicContentSize].width;

    [self addSubview:_symbolLabel];
    [_symbolLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

    [NSLayoutConstraint activateConstraints:@[
        [[self centerXAnchor] constraintEqualToAnchor:[_symbolLabel centerXAnchor]],
        [[self centerYAnchor] constraintEqualToAnchor:[_symbolLabel centerYAnchor] constant:1],
    ]];

    return self;
}

+ (instancetype)fieldWithModifiers:(STZFlags)flags target:(id)target action:(SEL)action {
    STZModifierField *field = [[self alloc] init];
    [field setFlags:flags];
    [field setTarget:target];
    [field setAction:action];
    [field sizeToFit];
    return field;
}

- (BOOL)needsPanelToBecomeKey {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return [self isEnabled] && [[NSApplication sharedApplication] isFullKeyboardAccessEnabled];
}

- (BOOL)becomeFirstResponder {
    return [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
    [self setEditing:NO];
    return [super resignFirstResponder];
}

- (void)mouseDown:(NSEvent *)event {
    if (![self isEnabled]) {
        return [super mouseDown:event];
    }

    if (!STZValidateFlags([event modifierFlags] & kSTZModifiersMask, NULL)) {
        [self setEditing:!_editing];
        [[self window] makeFirstResponder:self];
    }
}

- (void)keyDown:(NSEvent *)event {
    if (![self isEnabled]) {
        return [super keyDown:event];
    }

    [self setDisplayedSymbols:_flags];

    if ([[event characters] isEqualToString:@"\r"]) {
        [self setEditing:!_editing];
    } else if ([[event characters] isEqualToString:@" "]) {
        [self setEditing:YES];
    } else {
        [self setEditing:NO];
        [super keyDown:event];
    }
}

- (void)flagsChanged:(NSEvent *)event {
    if (!_editing || ![self isEnabled]) {
        return [super flagsChanged:event];
    }

    STZFlags flags = STZValidateFlags([event modifierFlags] & kSTZModifiersMask, NULL);

    if (flags) {
        _accumulatedFlags |= flags;
        [self setDisplayedSymbols:_accumulatedFlags];

    } else if (_accumulatedFlags) {
        _flags = _accumulatedFlags;
        [self setEditing:NO];

        if ([self action]) {
            [self sendAction:[self action] to:[self target]];
        }
    }
}

- (void)otherMouseDown:(NSEvent *)event {
    if (!_editing || ![self isEnabled]) {
        return [super otherMouseDown:event];
    }

    STZFlags flags = STZValidateFlags([event buttonNumber] & kSTZMouseButtonsMask, NULL);

    if (flags) {
        _flags = flags;
        [self setEditing:NO];

        if ([self action]) {
            [self sendAction:[self action] to:[self target]];
        }
    }
}

- (void)setFlags:(STZFlags)flags {
    if (flags == _flags) {return;}
    CFStringRef description;
    _flags = STZValidateFlags(flags, &description);
    [_symbolLabel setStringValue:NSLocalizedString((__bridge id)description, nil)];
}

- (void)setDisplayedSymbols:(STZFlags)flags {
    CFStringRef description;
    STZValidateFlags(flags, &description);
    [_symbolLabel setStringValue:NSLocalizedString((__bridge id)description, nil)];
}

- (void)setEditing:(BOOL)editing {
    [self setDisplayedSymbols:_flags];
    [self setHighlighted:(_editing = editing)];
    _accumulatedFlags = 0;
}

- (void)setEnabled:(BOOL)enabled {
    [super setEnabled:enabled];
    [_symbolLabel setTextColor:enabled ? [NSColor labelColor] : [NSColor disabledControlTextColor]];
}

- (BOOL)isHighlighted {
    return _highlighted;
}

- (void)setHighlighted:(BOOL)highlighted {
    [super setHighlighted:highlighted];
    [self setNeedsDisplay];
    _highlighted = highlighted;
}

- (NSRect)focusRingMaskBounds {
    return [self bounds];
}

- (void)drawFocusRingMask {
    if (@available(macOS 26, *)) {
        [[NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                         xRadius:TAHOE_CONTROL_RADIUS
                                         yRadius:TAHOE_CONTROL_RADIUS] fill];
    } else {
        [[NSImage imageNamed:ACImageNameModifierFieldBezelMask] drawInRect:[self bounds]];
    }
}

- (void)viewWillDraw {
    if ([self isEnabled]) {
        BOOL emphasized = [self isHighlighted] && [[self window] isKeyWindow];
        [_symbolLabel setTextColor:emphasized ? [NSColor alternateSelectedControlTextColor] : [NSColor labelColor]];
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    BOOL emphasized = [self isHighlighted] && [[self window] isKeyWindow];

    if (@available(macOS 26, *)) {
        if (!emphasized) {
            NSColor *borderColor = [NSColor separatorColor];
            CGFloat alpha = [borderColor alphaComponent];
            [[borderColor colorWithAlphaComponent:[self isEnabled] ? alpha : alpha / 2] setStroke];

            NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect([self bounds], 0.5, 0.5)
                                                                 xRadius:TAHOE_CONTROL_RADIUS - 0.5
                                                                 yRadius:TAHOE_CONTROL_RADIUS - 0.5];
            [path setLineWidth:1];
            [path stroke];

        } else {
            [[NSColor controlAccentColor] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                             xRadius:TAHOE_CONTROL_RADIUS
                                             yRadius:TAHOE_CONTROL_RADIUS] fill];
        }

    } else {
        if (!emphasized) {
            NSImage *bezel = [NSImage imageNamed:ACImageNameModifierFieldBezelOff];
            [bezel drawInRect:[self bounds]
                     fromRect:(NSRect){NSZeroPoint, [bezel size]}
                    operation:NSCompositingOperationSourceOver
                     fraction:[self isEnabled] ? 1 : 0.5];

        } else {
            NSImage *backingImage = nil;

            BOOL hasOwnBacking = [self layer] != nil;
            if (!hasOwnBacking) {
                backingImage = [[NSImage alloc] initWithSize:[self bounds].size];
                [backingImage lockFocus];
            }

            NSImage *bezel = [NSImage imageNamed:ACImageNameModifierFieldBezelOn];
            [bezel drawInRect:[self bounds]
                     fromRect:(NSRect){NSZeroPoint, [bezel size]}
                    operation:NSCompositingOperationSourceOver
                     fraction:1];

            if (@available(macOS 10.14, *)) {
                [[NSColor controlAccentColor] setFill];
            } else {
                [[NSColor colorForControlTint:[NSColor currentControlTint]] setFill];
            }
            NSRectFillUsingOperation([self bounds], NSCompositingOperationColor);

            NSImage *mask = [NSImage imageNamed:ACImageNameModifierFieldBezelMask];
            [mask drawInRect:[self bounds]
                     fromRect:(NSRect){NSZeroPoint, [mask size]}
                    operation:NSCompositingOperationDestinationIn
                     fraction:1];

            if (backingImage) {
                [backingImage unlockFocus];
                [backingImage drawInRect:[self bounds]];
            }
        }
    }
}

- (NSLayoutYAxisAnchor *)firstBaselineAnchor {
    return [_symbolLabel firstBaselineAnchor];
}

- (NSLayoutYAxisAnchor *)lastBaselineAnchor {
    return [_symbolLabel lastBaselineAnchor];
}

- (NSSize)intrinsicContentSize {
    if (@available(macOS 26, *)) {
        return NSMakeSize(_symbolMaxWidth + 8, 21);
    } else {
        return NSMakeSize(_symbolMaxWidth + 8, 19);
    }
}

@end
