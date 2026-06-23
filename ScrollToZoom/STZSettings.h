/*
 *  STZSettings.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#pragma once
#include <CoreGraphics/CGEvent.h>
#include "STZCommon.h"

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN


typedef OPTION_FLAGS(uint32_t) {
    kSTZMagicZoomEnabled        = 1 << 0,
    kSTZTriggerFlagsEnabled     = 1 << 1,

    kSTZPracticalModesMask      = kSTZMagicZoomEnabled | kSTZTriggerFlagsEnabled,

    /// Dictatorship means that ScrollToZoom inspects scroll wheel events during the HID phase
    /// before any other mutating event taps receive them, and modifies the final events during the
    /// annotated session phase after other taps have processed them. This is essential for working
    /// seamlessly with other mouse optimization tools. However, this mode may cause delays in event
    /// delivery and increases power consumption.
    kSTZWantsDictatorship       = 1 << 2,
} STZModes;

STZModes STZGetPreferredModes(void);
void STZSetPreferredModes(STZModes);


/// This value is not affected by the mode flag `kSTZTriggerFlagsEnabled`.
STZFlags STZGetTriggerFlags(void);
void STZSetTriggerFlags(STZFlags);

/// The factor to calculate the relative magnification from the scroll wheel delta.
double STZGetMagnificationScalar(void);
void STZSetMagnificationScalar(double);

/// A factor to reduce relative magnification during the inertia phase.
///
/// Let `k` be `1 - STZMomentumZoomAttenuation`, the additional multiplier applied to the
/// magnification factor is calculated as `k ^ (∆t / k)`, where `∆t` is the time in seconds since
/// the beginning of the inertia phase (i.e., when the user lifts their fingers).
///
/// If this value is set to `1`, the inertia effect has no effect.
double STZGetMomentumZoomAttenuation(void);
void STZSetMomentumZoomAttenuation(double);

/// The minimum magnification threshold during the inertia phase.
///
/// If an event in the inertia phase would generate a magnification factor smaller than this value,
/// the event and all subsequent inertia phase events will be ignored.
///
/// This value, along with `STZMomentumZoomMinValue`, can be used to trim the inertia phase to
/// prevent it being too long. Normal zoom gestures have no inertia phase; normal scroll’s inertia
/// phase will be ignored by AppKit if needed (when reaching the boundary of the document). However,
/// for zoom events converted from scroll events, if the inertia phase is ignored, the zoom will be
/// stopped abruptly; if the inertia phase is too long, the final effect will be too sloppy.
double STZGetMomentumZoomMinValue(void);
void STZSetMomentumZoomMinValue(double);


typedef OPTION_FLAGS(uint32_t) {
    kSTZDisabledForApp          = 1 << 0,
    kSTZFlagsExcludedForApp     = 1 << 1,
} STZAppOptions;

STZAppOptions STZGetAppOptionsForBundleIdentifier(CFStringRef __nullable bundleID);
void STZSetAppOptionsForBundleIdentifier(CFStringRef bundleID, STZAppOptions);

/// Posted to the local center when `STZSetAppOptionsForBundleIdentifier` is called.
/// The user info has a key `bundleIdentifier`, indicating options for what have changed.
extern CFStringRef const kSTZAppOptionsDidChangeNotification;

/// Returns all tap options keyed by the bundle identifier. The value is a raw pointer whose bit
/// pattern represents the options.
CFDictionaryRef STZCopyOptionsForAllApps(void);

STZAppOptions STZGetRecommendedAppOptionsForBundleIdentifier(CFStringRef bundleID);


CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED
