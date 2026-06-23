/*
 *  STZEventHandling.h
 *  ScrollToZoom
 *
 *  Created by alpha on 2026/6/19.
 *  Copyright © 2026 alphaArgon.
 */

#pragma once
#include "STZSettings.h"


STZModes STZGetWorkingModes(void);
bool STZSetWorkingModes(STZModes);


extern CFStringRef const kSTZWorkingModesDidChangeNotification;
