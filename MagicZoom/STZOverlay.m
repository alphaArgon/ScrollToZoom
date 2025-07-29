/*
 *  STZOverlay.c
 *  MagicZoom
 *
 *  Created by alpha on 2025/5/6.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZOverlay.h"
#import <Foundation/Foundation.h>


bool STZIsLoggingEnabled(void) {
    return false;
}

void STZDebugLog(char const *message, ...) {
    return;
}

void STZUnknownEnumCase(char const *type, int64_t value) {
    return;
}


double STZScrollToZoomMagnifier = 0.0025;
double STZScrollMomentumToZoomAttenuation = 0.8;
double STZScrollMinMomentumMagnification = 0.001;


static NSString *const STZScrollToZoomMagnifierKey = @"STZScrollToZoomMagnifier";
static NSString *const STZScrollMomentumToZoomAttenuationKey = @"STZScrollMomentumToZoomAttenuation";
static NSString *const STZScrollMinMomentumMagnificationKey = @"STZScrollMinMomentumMagnification";


static double clamp(double x, double lo, double hi) {
    //  `NaN` gives average of `lo` and `hi`;
    if (x != x) {return (lo + hi) / 2;}
    return x < lo ? lo : x > hi ? hi : x;
}


double STZGetScrollToZoomMagnifier(void) {
    return STZScrollToZoomMagnifier;
}

void STZSetScrollToZoomMagnifier(double magnifier) {
    STZScrollToZoomMagnifier = clamp(magnifier, -1, 1);

    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollToZoomMagnifier
                                              forKey:STZScrollToZoomMagnifierKey];
    STZDebugLog("ScrollToZoomMagnifier set to %f", STZScrollToZoomMagnifier);
}


double STZGetScrollMomentumToZoomAttenuation(void) {
    return STZScrollMomentumToZoomAttenuation;
}

void STZSetScrollMomentumToZoomAttenuation(double attenuation) {
    STZScrollMomentumToZoomAttenuation = clamp(attenuation, 0, 1);

    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollMomentumToZoomAttenuation
                                              forKey:STZScrollMomentumToZoomAttenuationKey];
    STZDebugLog("ScrollMomentumToZoomAttenuation set to %f", attenuation);
}


double STZGetScrollMinMomentumMagnification(void) {
    return STZScrollMinMomentumMagnification;
}

void STZSetScrollMinMomentumMagnification(double minMagnification) {
    STZScrollMinMomentumMagnification = clamp(minMagnification, 0, 1);

    [[NSUserDefaults standardUserDefaults] setDouble:STZScrollMinMomentumMagnification
                                              forKey:STZScrollMinMomentumMagnificationKey];
    STZDebugLog("ScrollMinMomentumMagnification set to %f", minMagnification);
}


void STZLoadArgumentsFromUserDefaults(void) {
    NSUserDefaults *userDefaults = [NSUserDefaults standardUserDefaults];

    NSNumber *magnifier = [userDefaults objectForKey:STZScrollToZoomMagnifierKey];
    if (magnifier && [magnifier isKindOfClass:[NSNumber self]]) {
        STZScrollToZoomMagnifier = clamp([magnifier doubleValue], -1, 1);
    }

    NSNumber *attenuation = [userDefaults objectForKey:STZScrollMomentumToZoomAttenuationKey];
    if (attenuation && [attenuation isKindOfClass:[NSNumber self]]) {
        STZScrollMomentumToZoomAttenuation = clamp([attenuation doubleValue], 0, 1);
    }

    NSNumber *minMomentum = [userDefaults objectForKey:STZScrollMinMomentumMagnificationKey];
    if (minMomentum && [minMomentum isKindOfClass:[NSNumber self]]) {
        STZScrollMinMomentumMagnification = clamp([minMomentum doubleValue], 0, 1);
    }

    STZDebugLog("Arguments loaded from user defaults:");
    STZDebugLog("\tScrollToZoomMagnifier set to %f", STZScrollToZoomMagnifier);
    STZDebugLog("\tScrollMomentumToZoomAttenuation set to %f", STZScrollMomentumToZoomAttenuation);
    STZDebugLog("\tScrollMinMomentumMagnification set to %f", STZScrollMinMomentumMagnification);
}
