/*
 *  STZControls.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZControls.h"
#import "STZEventTap.h"
#import "GeneratedAssetSymbols.h"
#import <Carbon/Carbon.h>


@implementation STZModifierField {
    BOOL                    _editing;
    BOOL                    _highlighted;
    NSEventModifierFlags    _accumulatedFlags;
    NSTextField            *_symbolLabel;
    CGFloat                 _symbolMaxWidth;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];

    CFStringRef description;
    STZValidateModifierFlags(~0, &description);
    _symbolLabel = [NSTextField labelWithString:(__bridge id)description];
    [_symbolLabel setRefusesFirstResponder:YES];
    [_symbolLabel setEditable:NO];
    [_symbolLabel setSelectable:NO];

    _symbolMaxWidth = [_symbolLabel intrinsicContentSize].width;

    [self addSubview:_symbolLabel];
    [_symbolLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

    [NSLayoutConstraint activateConstraints:@[
        [[self centerXAnchor] constraintEqualToAnchor:[_symbolLabel centerXAnchor]],
        [[self centerYAnchor] constraintEqualToAnchor:[_symbolLabel centerYAnchor] constant:1],
    ]];

    return self;
}

+ (instancetype)fieldWithModifiers:(NSEventModifierFlags)modifiers target:(id)target action:(SEL)action {
    STZModifierField *field = [[self alloc] init];
    [field setModifiers:modifiers];
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

    if (!STZValidateModifierFlags((CGEventFlags)[event modifierFlags], NULL)) {
        [self setEditing:!_editing];
        [[self window] makeFirstResponder:self];
    }
}

- (void)keyDown:(NSEvent *)event {
    if (![self isEnabled]) {
        return [super keyDown:event];
    }

    [self setDisplayedSymbols:[self modifiers]];

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

    NSEventModifierFlags flags = (NSEventModifierFlags)STZValidateModifierFlags((CGEventFlags)[event modifierFlags], NULL);

    if (flags) {
        _accumulatedFlags |= flags;
        [self setDisplayedSymbols:_accumulatedFlags];

    } else if (_accumulatedFlags) {
        _modifiers = _accumulatedFlags;
        [self setEditing:NO];

        if ([self action]) {
            [self sendAction:[self action] to:[self target]];
        }
    }
}

- (void)setModifiers:(NSEventModifierFlags)modifiers {
    if (modifiers == _modifiers) {return;}
    CFStringRef description;
    _modifiers = (NSEventModifierFlags)STZValidateModifierFlags((CGEventFlags)modifiers, &description);
    [_symbolLabel setStringValue:(__bridge id)description];
}

- (void)setDisplayedSymbols:(NSEventModifierFlags)modifiers {
    CFStringRef description;
    STZValidateModifierFlags((CGEventFlags)modifiers, &description);
    [_symbolLabel setStringValue:(__bridge id)description];
}

- (void)setEditing:(BOOL)editing {
    [self setModifiers:_modifiers];
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
    [[NSImage imageNamed:ACImageNameModifierFieldBezelMask] drawInRect:[self bounds]];
}

- (void)viewWillDraw {
    BOOL emphasized = [self isHighlighted] && [[self window] isKeyWindow];
    [_symbolLabel setTextColor:emphasized ? [NSColor alternateSelectedControlTextColor] : [NSColor labelColor]];
}

- (void)drawRect:(NSRect)dirtyRect {
    BOOL emphasized = [self isHighlighted] && [[self window] isKeyWindow];

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

- (NSLayoutYAxisAnchor *)firstBaselineAnchor {
    return [_symbolLabel firstBaselineAnchor];
}

- (NSLayoutYAxisAnchor *)lastBaselineAnchor {
    return [_symbolLabel lastBaselineAnchor];
}

- (NSSize)intrinsicContentSize {
    return NSMakeSize(_symbolMaxWidth + 8, 19);
}

@end
