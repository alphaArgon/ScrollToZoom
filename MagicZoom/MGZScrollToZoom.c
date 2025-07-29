/*
 *  MGZScrollToZoom.m
 *  MagicZoom
 *
 *  Created by alpha on 2025/7/29.
 *  Copyright © 2025 alphaArgon.
 */

#import "MGZScrollToZoom.h"
#import "STZDotDashDrag.h"
#import "STZOverlay.h"
#import "CGEventSPI.h"
#import <ApplicationServices/ApplicationServices.h>


typedef enum : uint8_t {
    kMGZNoConvert,
    kMGZMakeSolidZoom,
    kMGZAwaitingMomentum,
    kMGZDiscardedMomentum,
} MGZConversion;


typedef struct {
    MGZConversion       conversion;
    uint64_t            timerToken;
    CGEventTimestamp    momentumStart;
    CGPoint             mouseLocation;
} MGZWheelContext;


static STZCScanCache _wheelContexts = kSTZCScanCacheEmptyForType(MGZWheelContext);


static void eventTapTimeout(void) {
    MGZSetScrollToZoomEnabled(false);
    STZDebugLog("Event tap disabled due to timeout");
}


static CFMachPortRef        tapPort = NULL;
static CFRunLoopSourceRef   tapSource = NULL;
static bool                 tapEnabled = false;


static void onDoubleTap(uint64_t registryID, bool active, void *refcon) {
    if (tapPort != NULL && active && !tapEnabled) {
        CGEventTapEnable(tapPort, tapEnabled = true);
    }
}


static CGEventRef tapCallback(CGEventTapProxy proxy, CGEventType eventType, CGEventRef event, void *userInfo) {
    switch (eventType) {
    case kCGEventTapDisabledByTimeout:      eventTapTimeout(); CF_FALLTHROUGH;
    case kCGEventTapDisabledByUserInput:    return NULL;
    default: assert(eventType == kCGEventScrollWheel); break;
    }

    printf("run\n");

    bool byMomentum;
    STZPhase phase = STZGetPhaseFromScrollWheelEvent(event, &byMomentum);

    uint64_t registryID = CGEventGetRegistryID(event);
    MGZWheelContext *context;

    if ((phase == kSTZPhaseBegan || phase == kSTZPhaseMayBegin) && !byMomentum
     && STZDotDashDragIsActiveWithinTimeout(registryID, 0.5 * NSEC_PER_SEC)) {
        //  Magic Mice don’t send events of this phase.
        if (phase == kSTZPhaseMayBegin) {return NULL;}

        STZCScanCacheResult result;
        context = STZCScanCacheGetDataForIdentifier(&_wheelContexts, registryID, true, &result);
        if (result == kSTZCScanCacheNewCreated || result == kSTZCScanCacheExpiredReused) {
            context->timerToken = 0;
        }
        context->conversion = kMGZMakeSolidZoom;
        context->mouseLocation = CGEventGetLocation(event);

    } else {
        context = STZCScanCacheGetDataForIdentifier(&_wheelContexts, registryID, false, NULL);
        if (context == NULL || !context->conversion) {
            CGEventTapEnable(tapPort, tapEnabled = false);
            return event;
        }

        if (phase == kSTZPhaseEnded || phase == kSTZPhaseCancelled) {
            context->conversion = kMGZNoConvert;

        } else if (byMomentum && context->conversion == kMGZDiscardedMomentum) {
            return NULL;

        } else if (phase == kSTZPhaseBegan && byMomentum && context->conversion == kMGZAwaitingMomentum) {
            context->momentumStart = CGEventGetTimestamp(event);
            context->conversion = kMGZMakeSolidZoom;
            phase = kSTZPhaseChanged;
        }
    }

    context->timerToken += 1;

    double delta = STZGetScrollWheelPrimaryDelta(event);
    delta *= STZIsScrollWheelFlipped(event) ? -1 : 1;

    double scale = delta * STZGetScrollToZoomMagnifier();
    if (byMomentum && context->conversion == kMGZMakeSolidZoom) {
        CGEventTimestamp now = CGEventGetTimestamp(event);
        double k = 1 - STZGetScrollMomentumToZoomAttenuation();
        double dt = (now - context->momentumStart) / (double)NSEC_PER_SEC;
        scale *= k != 0 ? pow(k, dt / k) : 0;

        if (fabs(scale) < STZGetScrollMinMomentumMagnification()) {
            context->conversion = kMGZDiscardedMomentum;
            phase = kSTZPhaseEnded;
            scale = 0;
        }
    }

    CGEventRef zoomEvent = STZCreateZoomGestureEvent(event);
    STZAdaptGestureEvent(zoomEvent, phase, scale);
    CGEventSetLocation(zoomEvent, context->mouseLocation);

    if (phase == kSTZPhaseEnded && !byMomentum) {
        context->conversion = kMGZAwaitingMomentum;

        uint64_t token = context->timerToken;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.05 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
            if (token != context->timerToken) {
                CFRelease(zoomEvent);
            } else {
                context->conversion = kMGZNoConvert;
                CGEventPost(kCGSessionEventTap, zoomEvent);
                CFRelease(zoomEvent);
            }
        });

        return NULL;
    }

    return zoomEvent;  //  Will be released by the system.
}


bool MGZGetScrollToZoomEnabled(void) {
    return tapPort != NULL;
}


bool MGZSetScrollToZoomEnabled(bool enable) {
    if (enable == (tapPort != NULL)) {
        return true;
    }

    if (!enable) {
        STZSetListeningMultitouchDevices(false);
        CFRunLoopRemoveSource(CFRunLoopGetMain(), tapSource, kCFRunLoopCommonModes);
        CGEventTapEnable(tapPort, tapEnabled = false);
        CFRelease(tapSource);
        CFRelease(tapPort);
        tapPort = NULL;
        tapSource = NULL;
        STZCScanCacheRemoveAll(&_wheelContexts);
        return true;
    }

    if (!AXIsProcessTrusted()) {return false;}
    if (!STZSetListeningMultitouchDevices(true)) {return false;}
    STZDotDashDragObserveActivation(onDoubleTap, NULL);

    tapPort = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                               kCGEventTapOptionDefault, 1 << kCGEventScrollWheel,
                               tapCallback, NULL);
    if (tapPort == NULL) {return false;}

    tapSource = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, tapPort, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), tapSource, kCFRunLoopCommonModes);

    if (!STZCScanCacheIsInUse(&_wheelContexts)) {
        CGEventTimestamp const DATA_LIFETIME = 300 * NSEC_PER_SEC;  //  5 minutes.
        CGEventTimestamp const CHECK_INTERVAL = 60 * NSEC_PER_SEC;  //  1 minute.
        STZCScanCacheSetDataLifetime(&_wheelContexts, DATA_LIFETIME, CHECK_INTERVAL);
    }

    return true;
}
