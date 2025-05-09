/*
 *  CGEventSPI.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreGraphics/CGEvent.h>
#import <IOKit/hid/IOHIDLib.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


enum {
    kCGEventZoom = 28,
    kCGEventGesture = 29,  //  NSEventTypeGesture
    kCGEventDockControl = 30,
    kCGEventFluidTouchGesture = 31,
};


enum {
    kCGGestureEventHIDType = 110,
    kCGGestureEventIsPreflight = 111,
    kCGGestureEventZoomValue = 113,
    kCGGestureEventRotationValue = 114,
    kCGGestureEventSwipeValue = 115,
    kCGGestureEventPreflightProgress = 116,
    kCGGestureEventStartEndSeriesType = 117,
    kCGGestureEventScrollX = 118,
    kCGGestureEventScrollY = 119,
    kCGGestureEventScrollZ = 120,
    kCGGestureEventSwipeMotion = 123,
    kCGGestureEventSwipeProgress = 124,
    kCGGestureEventSwipePositionX = 125,
    kCGGestureEventSwipePositionY = 126,
    kCGGestureEventSwipeVelocityX = 129,
    kCGGestureEventSwipeVelocityY = 130,
    kCGGestureEventSwipeVelocityZ = 131,
    kCGGestureEventPhase = 132,
    kCGGestureEventMask = 133,
    kCGGestureEventSwipeMask = 134,
    kCGScrollEventGestureFlagBits = 135,
    kCGSwipeEventGestureFlagBits = 136,
    kCGGestureEventFlavor = 138,
    kCGGestureEventZoomDeltaX = 139,
    kCGGestureEventZoomDeltaY = 140,
    kCGGestureEventProgress = 142,
    kCGGestureEventStage = 143,
    kCGGestureEventBehavior = 144,

    //  Reverse engineered from `-[NSEvent isDirectionInvertedFromDevice]`.
    kCGScrollEventIsDirectionInverted = 137,

    //  Thanks to https://stackoverflow.com/questions/219623 .
    kCGEventRegistryID = 87,
};


typedef CF_ENUM(uint32_t, CGHIDEventType) {
    kCGHIDEventTypeGestureStarted = 61,
    kCGHIDEventTypeGestureEnded = 62,
};


typedef struct CF_BRIDGED_TYPE(id) __IOHIDEvent *IOHIDEventRef;
IOHIDEventRef __nullable CGEventCopyIOHIDEvent(CGEventRef);


typedef CF_ENUM(uint32_t, IOHIDEventType) {
    kIOHIDEventTypeNULL,
    kIOHIDEventTypeVendorDefined,
    kIOHIDEventTypeKeyboard = 3,
    kIOHIDEventTypeRotation = 5,
    kIOHIDEventTypeScroll = 6,
    kIOHIDEventTypeZoom = 8,
    kIOHIDEventTypeDigitizer = 11,
    kIOHIDEventTypeNavigationSwipe = 16,
    kIOHIDEventTypeForce = 32,
};

IOHIDEventType IOHIDEventGetType(IOHIDEventRef);


typedef CF_ENUM(uint32_t, IOHIDEventField) {
    kIOHIDEventFieldScrollX = (kIOHIDEventTypeScroll << 16) | 0,
    kIOHIDEventFieldScrollY = (kIOHIDEventTypeScroll << 16) | 1,
};

double IOHIDEventGetFloatValue(IOHIDEventRef, IOHIDEventField);
void IOHIDEventSetFloatValue(IOHIDEventRef, IOHIDEventField, double);

//  This should return the same as value of `kCGEventRegistryID`.
uint64_t IOHIDEventGetSenderID(IOHIDEventRef);
void IOHIDEventSetSenderID(IOHIDEventRef, uint64_t);


static inline uint64_t CGEventGetRegistryID(CGEventRef event) {
    uint64_t registryID = CGEventGetIntegerValueField(event, kCGEventRegistryID);
    if (registryID) {return registryID;}

    IOHIDEventRef ioEvent = CGEventCopyIOHIDEvent(event);
    if (!ioEvent) {return 0;}

    uint64_t senderID = IOHIDEventGetSenderID(ioEvent);
    IOHIDEventSetSenderID(ioEvent, senderID);
    CFRelease(ioEvent);
    return senderID;
}


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
