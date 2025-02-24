/*
 *  STZControls.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZControls.h"


static NSEventModifierFlags checkModifiers(NSEventModifierFlags flags, NSString **description) {
    static struct {
        uint16_t                symbol;
        CFStringRef             name;
        NSEventModifierFlags    flag;
    } const items[] = {
        {u'⌃', CFSTR("Control"), NSEventModifierFlagControl},
        {u'⌥', CFSTR("Option"), NSEventModifierFlagOption},
        {u'⇧', CFSTR("Shift"), NSEventModifierFlagShift},
        {u'⌘', CFSTR("Command"), NSEventModifierFlagCommand}
    };

    static const size_t itemCount = sizeof(items) / sizeof(*items);

    NSEventModifierFlags checked = 0;
    uint16_t characters[itemCount];
    size_t characterCount = 0;

    for (size_t i = 0; i < sizeof(items) / sizeof(*items); ++i) {
        if (flags & items[i].flag) {
            checked |= items[i].flag;
            characters[characterCount] = items[i].symbol;
            characterCount += 1;
        }
    }

    if (description) {
        *description = [NSString stringWithCharacters:characters length:characterCount];
    }

    return checked;
}


@implementation STZModifierField {
    CGFloat                 _maxWidth;
    NSEventModifierFlags    _prevFlags;
}

- (instancetype)initWithFrame:(NSRect)frame {
    self = [super initWithFrame:frame];
    
    [self setEditable:NO];
    [self setSelectable:NO];
    [self setAlignment:NSTextAlignmentCenter];
    [self setBezelStyle:NSTextFieldRoundedBezel];
    [self setTextColor:[NSColor controlTextColor]];
    [self setFont:[NSFont systemFontOfSize:0]];

    [self setModifiers:~0];
    _maxWidth = [super intrinsicContentSize].width;

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

+ (NSEventModifierFlags)validateModifiers:(NSEventModifierFlags)modifiers {
    return checkModifiers(modifiers, NULL);
}

- (NSRect)focusRingMaskBounds {
    return [self bounds];
}

- (BOOL)needsPanelToBecomeKey {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return [self isEnabled];
}

- (BOOL)becomeFirstResponder {
    _prevFlags = 0;
    return [super becomeFirstResponder];
}

- (BOOL)resignFirstResponder {
    _prevFlags = 0;
    return [super resignFirstResponder];
}

- (void)flagsChanged:(NSEvent *)event {
    if (![self isEnabled]) {
        return [super flagsChanged:event];
    }

    NSEventModifierFlags flags = checkModifiers([event modifierFlags], NULL);

    if (flags == 0) {
        _prevFlags = 0;
    } else {
        _prevFlags |= flags;
        [self setModifiers:_prevFlags];
        [self sendAction:[self action] to:[self target]];
    }
}

- (void)setModifiers:(NSEventModifierFlags)modifiers {
    if (modifiers == _modifiers) {return;}

    NSString *description = nil;
    _modifiers = checkModifiers(modifiers, &description);
    [self setStringValue:description];
}

- (NSSize)intrinsicContentSize {
    NSSize size = [super intrinsicContentSize];
    size.width = _maxWidth;
    return size;
}

@end
