/*
 *  STZService.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreGraphics/CGEvent.h>

CF_IMPLICIT_BRIDGING_ENABLED


extern CGEventFlags STZScrollToZoomFlags;

/// The factor to calculate the relative magnification factor from the scroll wheel delta.
extern double STZScrollToZoomMagnifier;

/// A factor to reduce relative magnification during the inertia phase.
///
/// Let `k` be `1 - STZScrollMomentumToZoomAttenuation`, the additional multiplier applied to the
/// magnification factor is calculated as `k ^ (∆t / k)`, where `∆t` is the time in seconds since
/// the beginning of the inertia phase (i.e., when the user lifts their fingers).
///
/// If this value is set to `1`, the inertia effect has no effect.
extern double STZScrollMomentumToZoomAttenuation;

/// The minimum magnification threshold during the inertia phase.
///
/// If an event in the inertia phase would generate a magnification factor smaller than this value,
/// the event and all subsequent inertia phase events will be ignored.
///
/// This value, along with `STZScrollMomentumToZoomAttenuation`, can be used to trim the inertia
/// phase to prevent it being too long. Normal zoom gestures have no inertia phase; normal scroll’s
/// inertia phase will be ignored by AppKit if needed (when reaching the boundary of the document).
/// However, for zoom events converted from scroll events, if the inertia phase is ignored, the zoom
/// will be stopped abruptly; if the inertia phase is too long, the final effect will be too sloppy.
extern double STZScrollMinMomentumMagnification;


bool STZGetEventTapEnabled(void);
bool STZSetEventTapEnabled(bool);


//  MARK: - SPI


enum {
    kCGEventGesture = 29,  //  NSEventTypeGesture
};

enum {
    kCGGestureEventHIDType = 110,
    kCGGestureEventZoomValue = 113,
    kCGGestureEventSwipeValue = 115,
    kCGGestureEventPhase = 132,
};


typedef struct CF_BRIDGED_TYPE(id) __IOHIDEvent *IOHIDEventRef;
IOHIDEventRef CGEventCopyIOHIDEvent(CGEventRef);


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

CF_IMPLICIT_BRIDGING_DISABLED
