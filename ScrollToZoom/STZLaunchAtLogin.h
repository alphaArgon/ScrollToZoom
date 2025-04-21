/*
 *  STZLaunchAtLogin.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/19.
 *  Copyright © 2025 alphaArgon.
 */

#import <CoreFoundation/CoreFoundation.h>


bool STZShouldEnableLaunchAtLogin(void);

bool STZGetLaunchAtLoginEnabled(void);
bool STZSetLaunchAtLoginEnabled(bool);  ///< Returns the same value as the getting function.
