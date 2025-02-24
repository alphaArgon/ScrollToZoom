/*
 *  STZControls.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN


@interface STZModifierField : NSTextField

+ (NSEventModifierFlags)validateModifiers:(NSEventModifierFlags)modifiers;

+ (instancetype)fieldWithModifiers:(NSEventModifierFlags)modifiers target:(nullable id)target action:(nullable SEL)action;
@property(nonatomic) NSEventModifierFlags modifiers;

@end


NS_ASSUME_NONNULL_END
