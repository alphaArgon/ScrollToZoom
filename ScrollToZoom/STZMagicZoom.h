/*
 *  STZMagicZoom.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/26.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CoreFoundation.h>
#import "STZCommon.h"

CF_ASSUME_NONNULL_BEGIN


bool STZIsListeningMagicMice(void);
bool STZSetListeningMagicMice(bool listen);

typedef void (*STZMagicZoomCallback)(uint64_t registryID, bool active, void *refcon);
void STZMagicZoomObserveActivation(STZMagicZoomCallback __nullable callback, void *__nullable refcon);

bool STZShouldBeginMagicZoom(uint64_t registryID);


CF_ASSUME_NONNULL_END
