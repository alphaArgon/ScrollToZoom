/*
 *  STZOverlay.h
 *  MagicZoom
 *
 *  Created by alpha on 2025/5/6.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CFString.h>
#import "STZCommon.h"


bool STZIsLoggingEnabled(void);
void STZDebugLog(char const *message, ...) CF_FORMAT_FUNCTION(1, 2);
void STZUnknownEnumCase(char const *type, int64_t value);


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


void STZLoadArgumentsFromUserDefaults(void);
