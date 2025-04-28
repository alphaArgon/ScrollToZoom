/*
 *  STZDotDashDrag.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/26.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CoreFoundation.h>
#import "STZCommon.h"

CF_ASSUME_NONNULL_BEGIN


bool STZGetListeningMultitouchDevices(void);
bool STZSetListeningMultitouchDevices(bool flag);

typedef void (*STZDashDotDragCallback)(uint64_t registryID, bool active, void *refcon);
void STZDotDashDragObserveActivation(STZDashDotDragCallback __nullable callback, void *__nullable refcon);

bool STZDotDashDragIsActiveWithinTimeout(uint64_t registryID, CGEventTimestamp timeout);


CF_ASSUME_NONNULL_END
