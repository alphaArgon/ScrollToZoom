/*
 *  main.m
 *  MagicZoom
 *
 *  Created by alpha on 2025/5/6.
 *  Copyright © 2025 alphaArgon.
 */

#import <AppKit/AppKit.h>
#import "AppDelegate.h"


int main(int argc, char const **argv) {
    NSApplication *application = [NSApplication sharedApplication];
    AppDelegate *delegate = [[AppDelegate alloc] init];
    [application setDelegate:delegate];
    return NSApplicationMain(argc, argv);
}
