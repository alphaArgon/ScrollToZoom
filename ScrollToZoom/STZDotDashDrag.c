/*
 *  STZDotDashDrag.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/26.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZDotDashDrag.h"
#import "MTSupportSPI.h"
#import <os/lock.h>


typedef struct {
    CFIndex             lastTouches;
    MTPoint             tapLocation;
    CGEventTimestamp    tapTimestamp;
    CFIndex             tapCount;
} STZTapContext;

#define kSTZTapContextEmpty (STZTapContext){0, (MTPoint){0, 0}, 0, 0}

static STZCScanCache tapContexts = kSTZCScanCacheEmptyForType(STZTapContext);
static os_unfair_lock tapContextLock = OS_UNFAIR_LOCK_INIT;


static void anyMouseAdded(void *refcon, io_iterator_t iterator);
static void anyMouseRemoved(void *refcon, io_iterator_t iterator);
static void removeAllMice(void);

static int magicMouseTouched(MTDeviceRef, MTTouch const *, CFIndex touchCount, CFTimeInterval timestamp, MTFrameID);


static IONotificationPortRef mouseNotificationPort = NULL;


bool STZGetListeningMultitouchDevices(void) {
    return mouseNotificationPort != NULL;
}


bool STZSetListeningMultitouchDevices(bool flag) {
    //  No need to initialize.
    static io_iterator_t addedIterator;
    static io_iterator_t removedIterator;

    if (flag == (mouseNotificationPort != NULL)) {return true;}

    if (!flag) {
        CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(mouseNotificationPort);
        CFRunLoopRemoveSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        IOObjectRelease(addedIterator);
        IOObjectRelease(removedIterator);
        IONotificationPortDestroy(mouseNotificationPort);
        mouseNotificationPort = NULL;
        removeAllMice();
        return true;
    }

    CFMutableDictionaryRef properties = IOServiceMatching("AppleMultitouchDevice" /* kIOHIDDeviceKey */);
    if (!properties) {return false;}

    int value = kHIDPage_GenericDesktop;
    CFNumberRef pageNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    CFDictionarySetValue(properties, CFSTR(kIOHIDDeviceUsagePageKey), pageNumber);
    CFRelease(pageNumber);

    value = kHIDUsage_GD_Mouse;
    CFNumberRef usageNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    CFDictionarySetValue(properties, CFSTR(kIOHIDDeviceUsageKey), usageNumber);
    CFRelease(usageNumber);

    CFAutorelease(properties);

    if (__builtin_available(macOS 12.0, *)) {
        mouseNotificationPort = IONotificationPortCreate(kIOMainPortDefault);
    } else {
        mouseNotificationPort = IONotificationPortCreate(kIOMasterPortDefault);
    }

    if (IOServiceAddMatchingNotification(mouseNotificationPort, kIOFirstMatchNotification,
                                         CFDictionaryCreateCopy(kCFAllocatorDefault, properties) /* consumed */,
                                         anyMouseAdded, NULL, &addedIterator) != KERN_SUCCESS) {
        IONotificationPortDestroy(mouseNotificationPort);
        mouseNotificationPort = NULL;
        return false;
    }

    if (IOServiceAddMatchingNotification(mouseNotificationPort, kIOWillTerminateNotification,
                                         CFDictionaryCreateCopy(kCFAllocatorDefault, properties) /* consumed */,
                                         anyMouseRemoved, NULL, &removedIterator) != KERN_SUCCESS) {
        //  Release the initialized `addedIterator`.
        IOObjectRelease(addedIterator);
        IONotificationPortDestroy(mouseNotificationPort);
        mouseNotificationPort = NULL;
        return false;
    }

    anyMouseAdded(NULL, addedIterator);
    anyMouseRemoved(NULL, removedIterator);
    CFRunLoopSourceRef source = IONotificationPortGetRunLoopSource(mouseNotificationPort);
    CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
    return true;
}


static CFMutableDictionaryRef addedMice = NULL;

static void const *uint64Key(uint64_t key) {
#if __LP64__
    return (void const *)key;
#else
    CFTypeRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &key);
    return CFAutorelease(number);
#endif
}


static void anyMouseAdded(void *refcon, io_iterator_t iterator) {
    if (!addedMice) {
#if __LP64__
        addedMice = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                              NULL, &kCFTypeDictionaryValueCallBacks);
#else
        addedMice = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);
#endif
    }

    io_object_t item;
    while ((item = IOIteratorNext(iterator))) {
        MTDeviceRef device = MTDeviceCreateFromService(item);
        if (device) {
            uint32_t family = 0;
            MTDeviceGetFamilyID(device, &family);
            if (family == kMTDeviceFmailyMagicMouse) {
                uint64_t registryID = 0;
                IORegistryEntryGetRegistryEntryID(item, &registryID);
                CFDictionarySetValue(addedMice, uint64Key(registryID), device);

                MTDeviceStart(device, 0);
                MTRegisterContactFrameCallback(device, magicMouseTouched);
            }

            CFRelease(device);
        }

        IOObjectRelease(item);
    }
}


static void anyMouseRemoved(void *refcon, io_iterator_t iterator) {
    io_object_t item;
    while ((item = IOIteratorNext(iterator))) {
        if (addedMice) {
            //  Removing the device will release it.
            //  Releasing the device will stop it.

            uint64_t registryID = 0;
            IORegistryEntryGetRegistryEntryID(item, &registryID);
            CFDictionaryRemoveValue(addedMice, uint64Key(registryID));
        }

        IOObjectRelease(item);
    }
}


static void removeAllMice(void) {
    if (addedMice) {
        CFRelease(addedMice);
        addedMice = NULL;
    }
}


//  MARK: -


static STZDashDotDragCallback activationCallback = NULL;
static void *activationCallbackRefcon = NULL;


void STZDotDashDragObserveActivation(STZDashDotDragCallback callback, void *refcon) {
    activationCallback = callback;
    activationCallbackRefcon = refcon;
}


static float pointDistance(MTPoint a, MTPoint b) {
    return hypot(a.x - b.x, a.y - b.y);
}


static bool isTapDotDashDrag(STZTapContext *context) {
    if (!context->lastTouches || context->tapCount != 2) {return false;}
    if (context->tapLocation.x < 0.1) {return false;}
    if (context->tapLocation.x > 0.9) {return false;}
    return true;
}


static int magicMouseTouched(MTDeviceRef device, MTTouch const *touch, CFIndex touchCount, CFTimeInterval frameTime, MTFrameID frame) {
    uint64_t registryID = 0;
    MTDeviceGetRegistryID(device, &registryID);

    //  This function might be called from other thread.
    os_unfair_lock_lock(&tapContextLock);

    if (!STZCScanCacheIsInUse(&tapContexts)) {
        CGEventTimestamp const DATA_LIFETIME = 300 * NSEC_PER_SEC;  //  5 minutes.
        CGEventTimestamp const CHECK_INTERVAL = 60 * NSEC_PER_SEC;  //  1 minute.
        STZCScanCacheSetDataLifetime(&tapContexts, DATA_LIFETIME, CHECK_INTERVAL);
    }

    STZCScanCacheResult result;
    STZTapContext *context = STZCScanCacheGetDataForIdentifier(&tapContexts, registryID, true, &result);

    switch (result) {
    case kSTZCScanCacheFound:
    case kSTZCScanCacheExpiredRestored:
        break;

    case kSTZCScanCacheNewCreated:
    case kSTZCScanCacheExpiredReused:
        *context = kSTZTapContextEmpty;
        break;
    }

    if (touchCount != context->lastTouches) {
        bool singleTap = touchCount == 1 && context->lastTouches == 0;
        bool wasActive = isTapDotDashDrag(context);

        if (!singleTap) {
            if (touchCount != 0) {
                context->tapTimestamp = -INFINITY;
                context->tapCount = 0;
            }

        } else {
            CGEventTimestamp now = CGEventTimestampNow();
            if (now - context->tapTimestamp > (0.25 * NSEC_PER_SEC)
             || pointDistance(touch->location, context->tapLocation) > 0.5) {
                context->tapTimestamp = now;
                context->tapLocation = touch->location;
                context->tapCount = 0;
            }

            context->tapCount += 1;
            if (context->tapCount == 3) {
                context->tapCount = 1;
            }
        }

        context->lastTouches = touchCount;

        if (isTapDotDashDrag(context) != wasActive) {
            os_unfair_lock_unlock(&tapContextLock);

            //  Must be sync for lock balance.
            dispatch_sync(dispatch_get_main_queue(), ^{
                if (activationCallback) {
                    activationCallback(registryID, !wasActive, activationCallbackRefcon);
                }
            });

            os_unfair_lock_lock(&tapContextLock);
        }
    }

    os_unfair_lock_unlock(&tapContextLock);
    return 0;
}


bool STZDotDashDragIsActiveWithinTimeout(uint64_t registryID, CGEventTimestamp timeout) {
    bool active = false;
    os_unfair_lock_lock(&tapContextLock);

    STZTapContext *context = STZCScanCacheGetDataForIdentifier(&tapContexts, registryID, false, NULL);
    if (context && isTapDotDashDrag(context)) {
        CGEventTimestamp now = CGEventTimestampNow();
        active = (now - context->tapTimestamp) < timeout;
    }

    os_unfair_lock_unlock(&tapContextLock);
    return active;
}
