/*
 *  STZConsolePanel.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/8.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZConsolePanel.h"
#import "GeneratedAssetSymbols.h"


@interface STZConsolePanel () <NSToolbarDelegate, NSTableViewDelegate, NSTableViewDataSource> @end

NSString *const STZPanelTitleToolbarItemIdentifier = @"STZPanelTitleToolbarItem";
NSString *const STZToggleLoggingToolbarItemIdentifier = @"STZToggleLoggingToolbarItem";
NSString *const STZClearLogsToolbarItemIdentifier = @"STZClearLogsToolbarItem";


BOOL STZConsoleSharedPanelExists = NO;


bool STZIsLoggingEnabled(void) {
    if (!STZConsoleSharedPanelExists) {return false;}
    if ([[STZConsolePanel sharedPanel] isLoggingPaused]) {return false;}
    return true;
}


void STZDebugLog(char const *message, ...) {
    if (!STZIsLoggingEnabled()) {return;}

    va_list args;
    va_start(args, message);
    NSString *msg = [NSString stringWithCString:message encoding:NSUTF8StringEncoding];
    NSString *log = [[NSString alloc] initWithFormat:msg arguments:args];
    va_end(args);

    [[STZConsolePanel sharedPanel] addLog:log];
}


void STZUnknownEnumCase(char const *type, int64_t value) {
    STZDebugLog("Unknown enum %s case %lld", type, value);
}


@implementation STZConsolePanel {
    BOOL                _sharedPanel;
    BOOL                _loggingEnabled;
    NSTableView        *_logList;
    NSMutableArray     *_logDates;
    NSMutableArray     *_logMessages;
    NSDateFormatter    *_dateFormatter;
    NSInteger           _maxLogCount;
    NSInteger           _reduceOnceCount;
    NSDate             *_recentDate;
}

+ (STZConsolePanel *)sharedPanel {
    static STZConsolePanel __weak *weakPanel = nil;
    if (weakPanel) {return weakPanel;}

    STZConsolePanel *panel = [[STZConsolePanel alloc] initWithContentRect:NSZeroRect
                                                                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                                  backing:NSBackingStoreBuffered
                                                                    defer:YES];

    CGFloat const width = 600;
    NSRect frame = NSInsetRect([[NSScreen mainScreen] visibleFrame], 20, 20);
    frame.origin.x = NSMaxX(frame) - width;
    frame.size.width = width;
    [panel setFrame:frame display:NO];

    panel->_sharedPanel = YES;
    [panel setReleasedWhenClosed:NO];

    weakPanel = panel;
    STZConsoleSharedPanelExists = YES;
    [panel toggleLoggingPaused:nil];  //  Enable logging by default

    return panel;
}

+ (void)orderFrontSharedPanel {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [[self sharedPanel] makeKeyAndOrderFront:nil];
}

- (void)dealloc {
    if (_sharedPanel) {
        STZConsoleSharedPanelExists = NO;
    }
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag {
    self = [super initWithContentRect:contentRect styleMask:style backing:backingStoreType defer:flag];
    [self setTitle:NSLocalizedString(@"scroll-to-zoom-console", nil)];
    [self setContentMinSize:NSMakeSize(200, 200)];
    [self setContentMaxSize:NSMakeSize(800, 1e5)];

    _logList = [[NSTableView alloc] init];
    [_logList setAutoresizingMask:NSViewWidthSizable];
    [_logList setRowSizeStyle:NSTableViewRowSizeStyleSmall];
    [_logList setUsesAlternatingRowBackgroundColors:YES];

    NSTableColumn *messageColumn = [[NSTableColumn alloc] initWithIdentifier:@"STZLogMessages"];
    [messageColumn setTitle:NSLocalizedString(@"message", nil)];
    [messageColumn setResizingMask:NSTableColumnAutoresizingMask];
    [_logList addTableColumn:messageColumn];
    [_logList sizeLastColumnToFit];

    NSTableColumn *dateColumn = [[NSTableColumn alloc] initWithIdentifier:@"STZLogDates"];
    [dateColumn setTitle:NSLocalizedString(@"time", nil)];
    [dateColumn setWidth:80];
    [dateColumn setResizingMask:NSTableColumnUserResizingMask];
    [_logList addTableColumn:dateColumn];

    [_logList setDelegate:self];
    [_logList setDataSource:self];
    [_logList setAllowsMultipleSelection:YES];

    NSScrollView *scrollView = [[NSScrollView alloc] init];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutohidesScrollers:YES];
    [scrollView setDocumentView:_logList];
    [self setContentView:scrollView];

    NSToolbar *toolbar = [[NSToolbar alloc] initWithIdentifier:@"STZLogControls"];
    [toolbar setDelegate:self];
    [toolbar setAllowsUserCustomization:NO];
    [toolbar setDisplayMode:NSToolbarDisplayModeIconOnly];
    [self setToolbar:toolbar];
    [self setTitleVisibility:NSWindowTitleHidden];

    if (@available(macOS 10.14, *)) {
        [toolbar setCenteredItemIdentifier:STZPanelTitleToolbarItemIdentifier];
    }

    _logDates = [[NSMutableArray alloc] init];
    _logMessages = [[NSMutableArray alloc] init];

    _dateFormatter = [[NSDateFormatter alloc] init];
    [_dateFormatter setDateFormat:@"HH:mm:ss.SSS"];

    _maxLogCount = 1200;
    _reduceOnceCount = 200;

    [self setLevel:NSFloatingWindowLevel];
    return self;
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar {
    return @[STZPanelTitleToolbarItemIdentifier,
             STZToggleLoggingToolbarItemIdentifier,
             STZClearLogsToolbarItemIdentifier,
             NSToolbarFlexibleSpaceItemIdentifier];
}

- (NSArray<NSToolbarItemIdentifier> *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar {
    return @[NSToolbarFlexibleSpaceItemIdentifier,
             STZPanelTitleToolbarItemIdentifier,
             NSToolbarFlexibleSpaceItemIdentifier,
             STZToggleLoggingToolbarItemIdentifier,
             STZClearLogsToolbarItemIdentifier];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSToolbarItemIdentifier)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag {
    if ([itemIdentifier isEqualToString:STZPanelTitleToolbarItemIdentifier]) {
        NSTextField *label = [NSTextField labelWithString:[self title]];
        [label setAllowsDefaultTighteningForTruncation:YES];
        [label setTextColor:[NSColor windowFrameTextColor]];

        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        [item setView:label];
        [item setVisibilityPriority:NSToolbarItemVisibilityPriorityLow];
        return item;
    }

    if ([itemIdentifier isEqualToString:STZToggleLoggingToolbarItemIdentifier]) {
        NSButton *button = [[NSButton alloc] init];
        [button setBezelStyle:NSTexturedRoundedBezelStyle];
        [button setTarget:self];
        [button setAction:@selector(toggleLoggingPaused:)];
        [button setButtonType:NSButtonTypeToggle];
        [self updateToggleButtonImage:button];
        [button sizeToFit];

        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        [item setView:button];
        return item;
    }

    if ([itemIdentifier isEqualToString:STZClearLogsToolbarItemIdentifier]) {
        NSButton *button = [[NSButton alloc] init];
        [button setBezelStyle:NSTexturedRoundedBezelStyle];
        [button setTarget:self];
        [button setAction:@selector(clearLogs:)];
        [button setImage:[NSImage imageNamed:ACImageNameClearLogs]];
        [button sizeToFit];

        NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];
        [item setView:button];
        return item;
    }

    return nil;
}

- (void)updateToggleButtonImage:(NSButton *)button {
    [button setState:_loggingEnabled];
    [button setImage:[NSImage imageNamed:_loggingEnabled ? ACImageNameStopLogging : ACImageNameStartLogging]];
}

- (void)addLog:(NSString *)message {
    if (!_loggingEnabled) {return;}

    NSScrollView *scrollView = [_logList enclosingScrollView];
    BOOL scrolledToEnd = NSMaxY([_logList frame]) - NSMaxY([scrollView documentVisibleRect]) < 100;

    if (!_recentDate) {
        _recentDate = [NSDate date];
        STZConsolePanel __weak *weakSelf = self;
        CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopCommonModes, ^{
            STZConsolePanel *this = weakSelf;
            if (!this) {return;}
            this->_recentDate = nil;
        });
    }

    [_logDates addObject:_recentDate];
    [_logMessages addObject:[message copy]];
    [_logList insertRowsAtIndexes:[NSIndexSet indexSetWithIndex:[_logDates count] - 1]
                    withAnimation:NSTableViewAnimationEffectNone];

    if ([_logDates count] > _maxLogCount) {
        NSRange range = NSMakeRange(0, _reduceOnceCount);
        [_logDates removeObjectsInRange:range];
        [_logMessages removeObjectsInRange:range];
        [_logList removeRowsAtIndexes:[NSIndexSet indexSetWithIndexesInRange:range]
                        withAnimation:NSTableViewAnimationEffectNone];
    }

    if (scrolledToEnd) {
        [_logList scrollPoint:NSMakePoint(0, NSMaxY([_logList frame]))];
    }
}

- (void)clearLogs:(id)sender {
    [_logDates removeAllObjects];
    [_logMessages removeAllObjects];
    [_logList reloadData];
}

- (void)toggleLoggingPaused:(id)sender {
    if (_loggingEnabled) {
        [self addLog:@"Console logging paused"];
    }

    _loggingEnabled = !_loggingEnabled;

    NSInteger index = [[[self toolbar] items] indexOfObjectPassingTest:^BOOL(NSToolbarItem *obj, NSUInteger i, BOOL *stop) {
        return [[obj itemIdentifier] isEqualToString:STZToggleLoggingToolbarItemIdentifier];
    }];

    if (index != NSNotFound) {
        [self updateToggleButtonImage:(NSButton *)[[[[self toolbar] items] objectAtIndex:index] view]];
    }

    if (_loggingEnabled) {
        [self addLog:@"Console logging started"];
    }
}

- (BOOL)isLoggingPaused {
    return !_loggingEnabled;
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView {
    return [_logDates count];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)column row:(NSInteger)row {
    NSTableCellView *cellView = [tableView makeViewWithIdentifier:[column identifier] owner:self];
    if (!cellView) {
        cellView = [[NSTableCellView alloc] init];
        [cellView setIdentifier:[column identifier]];
        [cellView setRowSizeStyle:NSTableViewRowSizeStyleCustom];

        NSTextField *label = [NSTextField labelWithString:@""];
        [label setLineBreakMode:NSLineBreakByTruncatingTail];
        [cellView addSubview:label];

        if ([[column identifier] isEqualToString:@"STZLogDates"]) {
            [label setFont:[NSFont monospacedDigitSystemFontOfSize:11 weight:NSFontWeightRegular]];
            if ([label userInterfaceLayoutDirection] == NSUserInterfaceLayoutDirectionRightToLeft) {
                [label setAlignment:NSTextAlignmentLeft];
            } else {
                [label setAlignment:NSTextAlignmentRight];
            }
        } else {
            [label setFont:[NSFont systemFontOfSize:11 weight:NSFontWeightRegular]];
        }

        [label setTranslatesAutoresizingMaskIntoConstraints:NO];
        [NSLayoutConstraint activateConstraints:@[
            [[label leadingAnchor] constraintEqualToAnchor:[cellView leadingAnchor] constant:2],
            [[label trailingAnchor] constraintEqualToAnchor:[cellView trailingAnchor] constant:-2],
            [[label centerYAnchor] constraintEqualToAnchor:[cellView centerYAnchor]],
        ]];
    }

    NSString *content;

    if ([[column identifier] isEqualToString:@"STZLogDates"]) {
        NSDate *date = [_logDates objectAtIndex:row];
        content = [_dateFormatter stringFromDate:date];
    } else {
        content = [_logMessages objectAtIndex:row];
    }

    [[[cellView subviews] firstObject] setStringValue:content];
    return cellView;
}

- (void)copy:(id)sender {
    NSMutableString *string = [NSMutableString string];
    __block BOOL first = YES;

    [[_logList selectedRowIndexes] enumerateIndexesUsingBlock:^(NSUInteger i, BOOL *stop) {
        if (first) {
            first = NO;
        } else {
            [string appendString:@"\n"];
        }

        [string appendString:[[_logMessages objectAtIndex:i] stringByReplacingOccurrencesOfString:@"\t" withString:@"    "]];
        [string appendString:@"\t"];
        [string appendString:[_dateFormatter stringFromDate:[_logDates objectAtIndex:i]]];
    }];

    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] setString:string forType:NSPasteboardTypeString];
    [[NSPasteboard generalPasteboard] setString:string forType:NSPasteboardTypeTabularText];
}

@end
