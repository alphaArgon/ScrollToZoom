/*
 *  MGZScrollToZoom.h
 *  MagicZoom
 *
 *  Created by alpha on 2025/7/29.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreGraphics/CGEvent.h>
#import "STZCommon.h"

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


bool MGZGetScrollToZoomEnabled(void);
bool MGZSetScrollToZoomEnabled(bool);  ///< Returns whether the given value equals the state after calling this function.


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
