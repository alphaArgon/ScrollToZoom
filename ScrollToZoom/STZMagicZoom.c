/*
 *  STZMagicZoom.c
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/26.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZMagicZoom.h"
#import "MTSupportSPI.h"
#import <os/lock.h>


typedef union {
    uint8_t         bitPattern;
    struct {
        bool        down: 1;
        bool        steady: 1;
        bool        tooFast: 1;
    };
} TouchStatus;


typedef struct {
    TouchStatus         touches[10];
    uint8_t             goodTouchCount;
    bool                isRecognized;
    uint8_t             tappedNTimes;
    MTPoint             tapLocation;
    CGEventTimestamp    tapTimestamp;
} TapContext;


static STZCacheRef tapContexts = NULL;
static os_unfair_lock tapContextLock = OS_UNFAIR_LOCK_INIT;


static void anyMouseAdded(void *refcon, io_iterator_t iterator);
static void anyMouseRemoved(void *refcon, io_iterator_t iterator);
static void removeAllMice(void);

static int magicMouseTouched(MTDeviceRef, MTTouch const *, CFIndex touchCount, CFTimeInterval timestamp, MTFrameID, void *refcon);


static IONotificationPortRef mouseNotificationPort = NULL;


bool STZIsListeningMagicMice(void) {
    return mouseNotificationPort != NULL;
}


bool STZSetListeningMagicMice(bool listen) {
    //  No need to initialize here.
    static io_iterator_t addedIterator;
    static io_iterator_t removedIterator;

    if (listen == (mouseNotificationPort != NULL)) {return true;}

    if (!listen) {
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

    if (IOServiceAddMatchingNotification(mouseNotificationPort, kIOTerminatedNotification,
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
            uint64_t registryID = 0;
            IORegistryEntryGetRegistryEntryID(item, &registryID);

            MTDeviceRef device = (void *)CFDictionaryGetValue(addedMice, uint64Key(registryID));
            if (device) {
                MTDeviceStop(device);
                CFDictionaryRemoveValue(addedMice, uint64Key(registryID));
            }
        }

        IOObjectRelease(item);
    }
}


static void stopDevice(void const *key, void const *value, void *context) {
    MTDeviceStop((void *)value);
}

static void removeAllMice(void) {
    if (addedMice) {
        CFDictionaryApplyFunction(addedMice, stopDevice, NULL);
        CFRelease(addedMice);
        addedMice = NULL;
    }
}


//  MARK: -


static STZMagicZoomCallback activationCallback = NULL;
static void *activationCallbackRefcon = NULL;


void STZMagicZoomObserveActivation(STZMagicZoomCallback callback, void *refcon) {
    activationCallback = callback;
    activationCallbackRefcon = refcon;
}


#define SQUARE(x) ((x) * (x))


static float pointDistanceSquare(MTPoint a, MTPoint b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return SQUARE(dx) + SQUARE(dy);
}


static float vectorLengthSquare(MTPoint vector) {
    return SQUARE(vector.x) + SQUARE(vector.y);
}


//  This function might be called from other threads.
static int magicMouseTouched(MTDeviceRef device, MTTouch const *touches, CFIndex touchCount, CFTimeInterval frameTime, MTFrameID frame, void *refcon) {
    uint64_t registryID = 0;
    MTDeviceGetRegistryID(device, &registryID);

    os_unfair_lock_lock(&tapContextLock);

    if (!tapContexts) {
        tapContexts = STZCacheCreate(sizeof(TapContext), 300 * NSEC_PER_SEC, NULL);
    }

    TapContext *context = STZCacheGetValue(tapContexts, registryID);
    if (!context) {
        TapContext newValue = {
            .touches = {{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}},
            .goodTouchCount = 0,
            .isRecognized = false,
            .tappedNTimes = 0,
            .tapLocation = {0, 0},
            .tapTimestamp = 0,
        };
        context = STZCacheSetValue(tapContexts, registryID, &newValue);
    }

    uint8_t goodTouchCount = 0;
    uint8_t lastTouchIndex = 0;

    for (int i = 0; i < touchCount; ++i) {
        int j = touches[i].fingerID;
        if (j >= 8) {continue;}

        if (touches[i].phase < kMTTouchPhaseDidDown
         || touches[i].phase > kMTTouchPhaseWillUp) {
            context->touches[j] = (TouchStatus){0};
            continue;
        }

        //  Exclude fingers on the edge.
        if (touches[i].phase == kMTTouchPhaseDidDown
         && touches[i].location.x > 0.05
         && touches[i].location.x < 0.95) {
            context->touches[j].down = true;
        }

        if (!context->touches[j].down) {continue;}
        if (context->touches[j].tooFast) {continue;}

        if (vectorLengthSquare(touches[i].velocity) > SQUARE(4)) {
            context->touches[j].tooFast = true;
            continue;
        }

        if (touches[i].zDensity > 0.375 && touches[i].zTotal > 0.25) {
            context->touches[j].steady = true;
        }

        if (context->touches[j].steady) {
            goodTouchCount += 1;
            lastTouchIndex = i;
        }
    }

    if (goodTouchCount != context->goodTouchCount) {
        bool singleTap = goodTouchCount == 1 && context->goodTouchCount == 0;
        if (!singleTap) {
            if (goodTouchCount != 0) {
                context->tapTimestamp = -INFINITY;
                context->tappedNTimes = 0;
            }

        } else {
            CGEventTimestamp now = CGEventTimestampNow();
            CGEventTimestamp delta = CGEventTimestampNow() - context->tapTimestamp;
            if (delta > (0.25 * NSEC_PER_SEC)
             || pointDistanceSquare(touches[lastTouchIndex].location, context->tapLocation) > SQUARE(0.25)) {
                context->tapLocation = touches[lastTouchIndex].location;
                context->tappedNTimes = 0;
            }

            //  Magic Mouse sends touches per ~0.011s. Taps too frequent could be mistouch.
            if (delta > (0.05 * NSEC_PER_SEC)) {
                context->tapTimestamp = now;
                context->tappedNTimes += 1;
                if (context->tappedNTimes == 3) {
                    context->tappedNTimes = 1;
                }
            }
        }

        context->goodTouchCount = goodTouchCount;

        bool recognized = goodTouchCount && context->tappedNTimes == 2;
        if (context->isRecognized != recognized) {
            context->isRecognized = recognized;
            os_unfair_lock_unlock(&tapContextLock);

            //  Must be sync for lock balance.
            dispatch_sync(dispatch_get_main_queue(), ^{
                if (activationCallback) {
                    activationCallback(registryID, recognized, activationCallbackRefcon);
                }
            });

            os_unfair_lock_lock(&tapContextLock);
        }

    } else if (goodTouchCount == 1 && context->tappedNTimes == 1) {
        //  If a finger moved during the touch, reset the tap count.
        if (pointDistanceSquare(touches[lastTouchIndex].location, context->tapLocation) > SQUARE(0.1)) {
            context->tapLocation = touches[lastTouchIndex].location;
            context->tappedNTimes = 0;
        }
    }

    os_unfair_lock_unlock(&tapContextLock);
    return 0;
}


bool STZShouldBeginMagicZoom(uint64_t registryID) {
    const CGEventTimestamp timeout = 0.5 * NSEC_PER_SEC;

    bool active = false;
    os_unfair_lock_lock(&tapContextLock);

    TapContext *context = tapContexts ? STZCacheGetValue(tapContexts, registryID) : NULL;
    if (context && context->isRecognized) {
        CGEventTimestamp now = CGEventTimestampNow();
        active = (now - context->tapTimestamp) < timeout;
    }

    os_unfair_lock_unlock(&tapContextLock);
    return active;
}
