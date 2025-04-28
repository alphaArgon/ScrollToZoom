/*
 *  STZScrollToZoom.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreGraphics/CGEvent.h>
#import "STZCommon.h"

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


/// Whether Scroll to Zoom is the only process that modify scroll wheel events.
bool STZIsEventTapExclusive(void);

bool STZGetScrollToZoomEnabled(void);
bool STZSetScrollToZoomEnabled(bool);  ///< Returns whether the given value equals the state after calling this function.


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
