/*
 *  STZControls.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/25.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>
#import "STZEventCommon.h"

NS_ASSUME_NONNULL_BEGIN


NSFont *STZSymbolsFontOfSize(CGFloat size);


@interface STZModifierField : NSControl

+ (instancetype)fieldWithModifiers:(STZFlags)flags target:(nullable id)target action:(nullable SEL)action;
@property(nonatomic) STZFlags flags;

@end


NS_ASSUME_NONNULL_END
