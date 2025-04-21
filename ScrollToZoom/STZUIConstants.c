/*
 *  STZUIConstants.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/21.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZUIConstants.h"


CGFloat const kSTZUIInlineSpacing = 7;
CGFloat const kSTZUISmallSpacing = 10;
CGFloat const kSTZUINormalSpacing = 20;
CGFloat const kSTZUILargeSpacing = 30;

CGFloat kSTZUICheckboxWidth;
CGFloat kSTZUISliderBaselineOffset;
CGFloat kSTZUIFixCheckboxTrailing;


__attribute__((constructor))
static void init(void) {
    if (__builtin_available(macOS 10.16, *)) {
        kSTZUICheckboxWidth = 20;
        kSTZUISliderBaselineOffset = 0;
        kSTZUIFixCheckboxTrailing = -5;
    } else {
        kSTZUICheckboxWidth = 18;
        kSTZUISliderBaselineOffset = -3;
        kSTZUIFixCheckboxTrailing = 0;
    }
}
