/*
 *  main.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/1/24.
 *  Copyright © 2025 alphaArgon.
 */

#import "AppDelegate.h"


int main(int argc, char const **argv) {
    NSApplication *application = [NSApplication sharedApplication];
    AppDelegate *delegate = [[AppDelegate alloc] init];
    [application setDelegate:delegate];
    return NSApplicationMain(argc, argv);
}
