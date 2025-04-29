/*
 *  STZSettings.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreGraphics/CGEvent.h>
#import "STZCommon.h"

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


STZFlags STZGetScrollToZoomFlags(void);
void STZSetScrollToZoomFlags(STZFlags);

/// The factor to calculate the relative magnification from the scroll wheel delta.
double STZGetScrollToZoomMagnifier(void);
void STZSetScrollToZoomMagnifier(double);

/// A factor to reduce relative magnification during the inertia phase.
///
/// Let `k` be `1 - STZScrollMomentumToZoomAttenuation`, the additional multiplier applied to the
/// magnification factor is calculated as `k ^ (∆t / k)`, where `∆t` is the time in seconds since
/// the beginning of the inertia phase (i.e., when the user lifts their fingers).
///
/// If this value is set to `1`, the inertia effect has no effect.
double STZGetScrollMomentumToZoomAttenuation(void);
void STZSetScrollMomentumToZoomAttenuation(double);

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
double STZGetScrollMinMomentumMagnification(void);
void STZSetScrollMinMomentumMagnification(double);

/// Whether double tap and scroll to zoom, a bonus for Magic Mouse, is enabled.
bool STZGetDotDashDragToZoomEnabled(void);
void STZSetDotDashDragToZoomEnabled(bool);

/// Posted to the local center when `STZSetDotDashDragToZoomEnabled` is called.
extern CFStringRef const kSTZDotDashDragToZoomEnabledDidChangeNotificationName;

typedef enum __attribute__((flag_enum)): uint32_t {
    kSTZEventTapDefaultOptions  = 0,
    kSTZEventTapDisabled        = 1 << 0,
    kSTZEventTapExcludeFlags    = 1 << 1,
} STZEventTapOptions;

STZEventTapOptions STZGetEventTapOptionsForBundleIdentifier(CFStringRef __nullable bundleID);
void STZSetEventTapOptionsForBundleIdentifier(CFStringRef bundleID, STZEventTapOptions);

/// Posted to the local center when `STZSetEventTapOptionsForBundleIdentifier` is called.
/// The user info has a key `bundleIdentifier`, indicating options for what have changed.
extern CFStringRef const kSTZEventTapOptionsForBundleIdentifierDidChangeNotificationName;

/// Returns all tap options keyed by the bundle identifier. The value is a raw pointer whose bit
/// pattern represents the options.
CFDictionaryRef STZCopyAllEventTapOptions(void);

STZEventTapOptions STZGetRecommendedEventTapOptionsForBundleIdentifier(CFStringRef bundleID);

void STZLoadArgumentsFromUserDefaults(void);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
