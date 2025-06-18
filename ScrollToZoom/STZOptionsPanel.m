/*
 *  STZOptionsPanel.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZOptionsPanel.h"
#import "STZSettings.h"
#import "STZUIConstants.h"


@interface STZOptionsViewController : NSSplitViewController <NSOutlineViewDelegate, NSOutlineViewDataSource> @end


__attribute__((objc_direct_members))
@interface STZArrayWrapper : NSObject

@property(nonatomic, readonly) NSMutableArray *array;

- (NSInteger)count;
- (id)objectAtIndex:(NSInteger)index;

- (void)addObject:(id)object;
- (void)removeAllObjects;

@end


__attribute__((objc_direct_members))
@interface STZApplicationEntry : NSObject

+ (STZApplicationEntry *)entryWithApplication:(NSRunningApplication *)app;
+ (STZApplicationEntry *)entryWithBundleIdentifier:(NSString *)bundleID;
+ (STZApplicationEntry *)finderEntry;

@property(nonatomic, readonly) NSString *bundleIdentifier;
@property(nonatomic, readonly) NSString *localizedName;
@property(nonatomic, readonly, nullable) NSImage *icon;

@property(nonatomic, readonly, getter=isFinder) BOOL finder;
@property(nonatomic, readonly, getter=isNameDefault) BOOL nameDefault;

+ (void)sortEntries:(NSMutableArray<STZApplicationEntry *> *)entries;
+ (void)diffEntries:(NSArray<STZApplicationEntry *> *)old
          toEntries:(NSArray<STZApplicationEntry *> *)new
        getRemovals:(NSIndexSet **)outRemovals
      andInsertions:(NSIndexSet **)outInsertions;

@end


@implementation STZOptionsPanel

static STZOptionsPanel __weak *STZSharedOptionsPanel = nil;

+ (STZOptionsPanel *)sharedPanel {
    if (STZSharedOptionsPanel) {return STZSharedOptionsPanel;}

    STZOptionsPanel *panel = [[STZOptionsPanel alloc] initWithContentRect:NSZeroRect
                                                                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                                  backing:NSBackingStoreBuffered
                                                                    defer:YES];
    [panel center];
    NSRect frame = [panel frame];
    frame.origin.x += 250;
    frame.origin.y -= 50;
    [panel setFrame:frame display:NO];

    [panel setReleasedWhenClosed:NO];
    STZSharedOptionsPanel = panel;
    return panel;
}

+ (void)orderFrontSharedPanel {
    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
    [[self sharedPanel] makeKeyAndOrderFront:nil];
}

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSWindowStyleMask)style
                            backing:(NSBackingStoreType)backingStoreType
                              defer:(BOOL)flag {
    self = [super initWithContentRect:contentRect
                            styleMask:style
                              backing:backingStoreType
                                defer:flag];
    [self setContentViewController:[[STZOptionsViewController alloc] init]];
    [self setTitle:[[self contentViewController] title]];
    return self;
}

@end


@implementation STZOptionsViewController {
    STZArrayWrapper    *_runningBundleIDs;
    STZArrayWrapper    *_configuredBundleIDs;
    NSOutlineView      *_entryList;
    NSButton           *_enabledCheckbox;
    NSButton           *_excludingFlagsCheckBox;
    NSTextField        *_recommendedLabel;
    BOOL                _changesMadeBySelf;
}

static void *STZRunningApplicationsKVO = &STZRunningApplicationsKVO;

- (void)dealloc {
    [[NSWorkspace sharedWorkspace] removeObserver:self
                                       forKeyPath:@"runningApplications"
                                          context:STZRunningApplicationsKVO];
}

- (instancetype)initWithNibName:(NSNibName)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil {
    self = [super initWithNibName:nil bundle:nil];
    [self setTitle:NSLocalizedString(@"options-for-apps", nil)];

    _configuredBundleIDs = [[STZArrayWrapper alloc] init];
    _runningBundleIDs = [[STZArrayWrapper alloc] init];

    [[NSWorkspace sharedWorkspace] addObserver:self
                                    forKeyPath:@"runningApplications"
                                       options:0
                                       context:STZRunningApplicationsKVO];

    _entryList = [[NSOutlineView alloc] init];
    [_entryList setAutoresizingMask:NSViewWidthSizable];
    [_entryList setRowSizeStyle:NSTableViewRowSizeStyleSmall];
    [_entryList setSelectionHighlightStyle:NSTableViewSelectionHighlightStyleSourceList];
    [_entryList setFloatsGroupRows:NO];
    [_entryList setHeaderView:nil];

    NSTableColumn *entryColumn = [[NSTableColumn alloc] initWithIdentifier:@"STZApplicationEntries"];
    [entryColumn setResizingMask:NSTableColumnAutoresizingMask];
    [_entryList addTableColumn:entryColumn];
    [_entryList setOutlineTableColumn:entryColumn];
    [_entryList sizeLastColumnToFit];

    [_entryList setDelegate:self];
    [_entryList setDataSource:self];
    [_entryList setAllowsEmptySelection:NO];

    NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    [scrollView setHasVerticalScroller:YES];
    [scrollView setAutohidesScrollers:YES];
    [scrollView setDrawsBackground:NO];
    [scrollView setDocumentView:_entryList];

    _enabledCheckbox = [NSButton checkboxWithTitle:@""
                                            target:self action:@selector(toggleEnabled:)];
    _excludingFlagsCheckBox = [NSButton checkboxWithTitle:NSLocalizedString(@"exclude-flags", nil)
                                                   target:self action:@selector(toggleExcludingFlags:)];
    NSTextField *messageLabel = [NSTextField wrappingLabelWithString:NSLocalizedString(@"exclude-flags-message", nil)];
    [messageLabel setFont:[NSFont toolTipsFontOfSize:0]];
    [messageLabel setTextColor:[NSColor secondaryLabelColor]];
    [messageLabel setSelectable:NO];

    _recommendedLabel = [NSTextField wrappingLabelWithString:@""];
    [_recommendedLabel setHidden:YES];
    [_recommendedLabel setFont:[messageLabel font]];
    [_recommendedLabel setTextColor:[messageLabel textColor]];

    NSView *panelView = [[NSView alloc] init];
    [panelView setSubviews:@[_enabledCheckbox, _excludingFlagsCheckBox, messageLabel, _recommendedLabel]];
    for (NSView *subviews in [panelView subviews]) {
        [subviews setTranslatesAutoresizingMaskIntoConstraints:NO];
    }

    NSViewController *listController = [[NSViewController alloc] init];
    [listController setView:scrollView];
    NSSplitViewItem *listItem = [NSSplitViewItem sidebarWithViewController:listController];
    [listItem setCanCollapse:NO];
    [listItem setMinimumThickness:158];
    [listItem setMaximumThickness:158];

    NSViewController *panelController = [[NSViewController alloc] init];
    [panelController setView:panelView];
    NSSplitViewItem *panelItem = [NSSplitViewItem splitViewItemWithViewController:panelController];
    [panelItem setCanCollapse:NO];
    [panelItem setMinimumThickness:250];
    [panelItem setMaximumThickness:250];

    [self addSplitViewItem:listItem];
    [self addSplitViewItem:panelItem];

    [NSLayoutConstraint activateConstraints:@[
        [[_enabledCheckbox topAnchor] constraintEqualToAnchor:[panelView topAnchor] constant:kSTZUILargeSpacing],
        [[_enabledCheckbox leadingAnchor] constraintEqualToAnchor:[panelView leadingAnchor] constant:kSTZUILargeSpacing],

        [[_excludingFlagsCheckBox topAnchor] constraintEqualToAnchor:[_enabledCheckbox bottomAnchor] constant:kSTZUISmallSpacing],
        [[_excludingFlagsCheckBox leadingAnchor] constraintEqualToAnchor:[_enabledCheckbox leadingAnchor]],

        [[messageLabel topAnchor] constraintEqualToAnchor:[_excludingFlagsCheckBox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[messageLabel leadingAnchor] constraintEqualToAnchor:[_excludingFlagsCheckBox leadingAnchor] constant:kSTZUICheckboxWidth],
        [[messageLabel trailingAnchor] constraintEqualToAnchor:[_excludingFlagsCheckBox trailingAnchor]],
        [[messageLabel trailingAnchor] constraintEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUILargeSpacing],

        [[_recommendedLabel topAnchor] constraintEqualToAnchor:[messageLabel bottomAnchor] constant:kSTZUIInlineSpacing],
        [[_recommendedLabel leadingAnchor] constraintEqualToAnchor:[messageLabel leadingAnchor]],
        [[_recommendedLabel trailingAnchor] constraintEqualToAnchor:[messageLabel trailingAnchor]],

        [[panelView heightAnchor] constraintEqualToConstant:300],
    ]];

    return self;
}

- (void)viewWillAppear {
    [self reloadData];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(didChangeForBundleIdentifier:)
                                                 name:(__bridge id)kSTZEventTapOptionsForBundleIdentifierDidChangeNotificationName
                                               object:nil];
}

- (void)viewWillDisappear {
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:(__bridge id)kSTZEventTapOptionsForBundleIdentifierDidChangeNotificationName
                                                  object:nil];
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context {
    if (context != STZRunningApplicationsKVO) {
        return [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
    }

    [self reloadData];
}

- (STZApplicationEntry *)selectedEntry {
    NSInteger row = [_entryList selectedRow];
    if (row == -1) {return NULL;}

    STZApplicationEntry *entry = [_entryList itemAtRow:row];
    if (![entry isKindOfClass:[STZApplicationEntry self]]) {return NULL;}

    return entry;
}

- (void)reloadData {
    STZApplicationEntry *selected = [self selectedEntry];

    NSArray *oldRunning = [[_runningBundleIDs array] copy];
    NSArray *oldConfigured = [[_configuredBundleIDs array] copy];

    [_runningBundleIDs removeAllObjects];
    [_configuredBundleIDs removeAllObjects];

    NSMutableSet *met = [NSMutableSet set];
    [_runningBundleIDs addObject:[STZApplicationEntry finderEntry]];
    [met addObject:[[STZApplicationEntry finderEntry] bundleIdentifier]];

    for (NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications]) {
        if (![met containsObject:[app bundleIdentifier]]
         && [app activationPolicy] == NSApplicationActivationPolicyRegular) {
            [met addObject:[app bundleIdentifier]];
            [_runningBundleIDs addObject:[STZApplicationEntry entryWithApplication:app]];
        }
    }

    NSDictionary *configured = (__bridge_transfer id)STZCopyAllEventTapOptions();
    for (NSString *bundleID in [configured keyEnumerator]) {
        if (![met containsObject:bundleID]) {
            [_configuredBundleIDs addObject:[STZApplicationEntry entryWithBundleIdentifier:bundleID]];
        }
    }

    [STZApplicationEntry sortEntries:[_runningBundleIDs array]];
    [STZApplicationEntry sortEntries:[_configuredBundleIDs array]];

    if ([oldRunning count] == 0) {
        [_entryList reloadData];
        [_entryList expandItem:nil expandChildren:YES];
    } else {
        [_entryList beginUpdates];
        NSTableViewAnimationOptions animation = NSTableViewAnimationSlideUp;

        NSIndexSet *removals, *insertions;
        [STZApplicationEntry diffEntries:oldRunning toEntries:[_runningBundleIDs array]
                             getRemovals:&removals andInsertions:&insertions];
        [_entryList removeItemsAtIndexes:removals inParent:_runningBundleIDs withAnimation:animation];
        [_entryList insertItemsAtIndexes:insertions inParent:_runningBundleIDs withAnimation:animation];

        if ([oldConfigured count] != 0 && [_configuredBundleIDs count] != 0) {
            [STZApplicationEntry diffEntries:oldConfigured toEntries:[_configuredBundleIDs array]
                                 getRemovals:&removals andInsertions:&insertions];
            [_entryList removeItemsAtIndexes:removals inParent:_configuredBundleIDs withAnimation:animation];
            [_entryList insertItemsAtIndexes:insertions inParent:_configuredBundleIDs withAnimation:animation];

        } else if ([_configuredBundleIDs count] != 0) {
            [_entryList insertItemsAtIndexes:[NSIndexSet indexSetWithIndex:1] inParent:NULL withAnimation:animation];
            [_entryList expandItem:_configuredBundleIDs];

        } else if ([oldConfigured count] != 0) {
            [_entryList removeItemsAtIndexes:[NSIndexSet indexSetWithIndex:1] inParent:NULL withAnimation:animation];
        }
        [_entryList endUpdates];
    }

    //  `-rowForItem:` uses `-isEqual:`.
    NSInteger row = selected ? [_entryList rowForItem:selected] : -1;
    if (row == -1) {row = 1;}

    [_entryList selectRowIndexes:[NSIndexSet indexSetWithIndex:row] byExtendingSelection:NO];
    [self loadControlValues];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldShowOutlineCellForItem:(id)item {
    return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldCollapseItem:(id)item {
    return NO;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item {
    return item == _configuredBundleIDs || item == _runningBundleIDs;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isGroupItem:(id)item {
    return item == _configuredBundleIDs || item == _runningBundleIDs;
}

- (BOOL)outlineView:(NSOutlineView *)outlineView shouldSelectItem:(id)item {
    return [item isKindOfClass:[STZApplicationEntry self]];
}

- (NSInteger)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item {
    if (item) {
        return [(STZArrayWrapper *)item count];
    } else {
        return [_configuredBundleIDs count] ? 2 : 1;
    }
}

- (id)outlineView:(NSOutlineView *)outlineView child:(NSInteger)index ofItem:(id)item {
    if (item) {
        return [(STZArrayWrapper *)item objectAtIndex:index];
    } else {
        return index == 0 ? _runningBundleIDs : _configuredBundleIDs;
    }
}

- (NSView *)outlineView:(NSOutlineView *)outlineView viewForTableColumn:(NSTableColumn *)column item:(id)item {
    NSTableCellView *cellView = [outlineView makeViewWithIdentifier:[column identifier] owner:self];
    if (!cellView) {
        cellView = [[NSTableCellView alloc] init];
        [cellView setIdentifier:[column identifier]];

        NSTextField *label = [NSTextField labelWithString:@""];
        [label setLineBreakMode:NSLineBreakByTruncatingTail];
        [cellView setTextField:label];
        [cellView addSubview:label];

        NSImageView *imageView = [[NSImageView alloc] init];
        [cellView addSubview:imageView];
    }

    if (item == _runningBundleIDs) {
        [[cellView textField] setStringValue:NSLocalizedString(@"running", nil)];
        [[cellView imageView] setImage:nil];
        [cellView setImageView:nil];

    } else if (item == _configuredBundleIDs) {
        [[cellView textField] setStringValue:NSLocalizedString(@"configured", nil)];
        [[cellView imageView] setImage:nil];
        [cellView setImageView:nil];

    } else {
        [[cellView textField] setStringValue:[(STZApplicationEntry *)item localizedName]];
        [cellView setImageView:[[cellView subviews] objectAtIndex:1]];
        [[cellView imageView] setImage:[(STZApplicationEntry *)item icon]];
    }

    return cellView;
}

- (void)outlineViewSelectionDidChange:(NSNotification *)notification {
    [self loadControlValues];
}

- (void)didChangeForBundleIdentifier:(NSNotification *)notification {
    if (_changesMadeBySelf) {return;}
    STZApplicationEntry *entry = [self selectedEntry];
    if (!entry) {return;}

    NSString *bundleID = [[notification userInfo] objectForKey:@"bundleIdentifier"];
    if ([bundleID isEqualToString:[entry bundleIdentifier]]) {
        [self loadControlValues];
    }
}

- (void)loadControlValues {
    STZApplicationEntry *entry = [self selectedEntry];
    if (!entry) {return;}

    [_enabledCheckbox setTitle:[NSString stringWithFormat:NSLocalizedString(@"enabled-for-%@", nil), [entry localizedName]]];
    STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier]);

    [_enabledCheckbox setState:!(options & kSTZEventTapDisabled)];
    [_excludingFlagsCheckBox setState:!!(options & kSTZEventTapExcludeFlags)];
    [_excludingFlagsCheckBox setEnabled:!(options & kSTZEventTapDisabled)];

    if (STZGetRecommendedEventTapOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier])
        & kSTZEventTapExcludeFlags) {
        [_recommendedLabel setHidden:NO];
        [_recommendedLabel setStringValue:NSLocalizedString(@"recommended-for-this-app", nil)];

    } else {
        [_recommendedLabel setHidden:YES];
    }
}

- (void)toggleEnabled:(id)sender {
    STZApplicationEntry *entry = [self selectedEntry];
    if (!entry) {return;}

    _changesMadeBySelf = YES;
    STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier]);
    options = [_enabledCheckbox state] ? options & ~kSTZEventTapDisabled : options | kSTZEventTapDisabled;
    STZSetEventTapOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier], options);
    [_excludingFlagsCheckBox setEnabled:!(options & kSTZEventTapDisabled)];
    _changesMadeBySelf = NO;
}

- (void)toggleExcludingFlags:(id)sender {
    STZApplicationEntry *entry = [self selectedEntry];
    if (!entry) {return;}

    _changesMadeBySelf = YES;
    STZEventTapOptions options = STZGetEventTapOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier]);
    options = [_excludingFlagsCheckBox state] ? options | kSTZEventTapExcludeFlags : options & ~kSTZEventTapExcludeFlags;
    STZSetEventTapOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier], options);
    _changesMadeBySelf = NO;
}

@end


@implementation STZArrayWrapper

- (instancetype)init {
    self = [super init];
    _array = [[NSMutableArray alloc] init];
    return self;
}

- (NSInteger)count {
    return [_array count];
}

- (id)objectAtIndex:(NSInteger)index {
    return [_array objectAtIndex:index];
}

- (void)addObject:(id)object {
    [_array addObject:object];
}

- (void)removeAllObjects {
    [_array removeAllObjects];
}

@end


@implementation STZApplicationEntry

+ (STZApplicationEntry *)entryWithApplication:(NSRunningApplication *)app {
    STZApplicationEntry *entry = [[STZApplicationEntry alloc] init];
    entry->_bundleIdentifier = [app bundleIdentifier];
    entry->_localizedName = [app localizedName];
    entry->_icon = [app icon];
    return entry;
}

+ (STZApplicationEntry *)entryWithBundleIdentifier:(NSString *)bundleID {
    NSRunningApplication *app = [[NSRunningApplication runningApplicationsWithBundleIdentifier:bundleID] firstObject];
    if (app) {return [STZApplicationEntry entryWithApplication:app];}

    STZApplicationEntry *entry = [[STZApplicationEntry alloc] init];
    entry->_bundleIdentifier = [bundleID copy];

    NSURL *url;

    if ((url = [[NSWorkspace sharedWorkspace] URLForApplicationWithBundleIdentifier:bundleID])) {
        NSString *localizedName = [[NSFileManager defaultManager] displayNameAtPath:[url path]];
        if ([localizedName hasSuffix:@".app"]) {
            localizedName = [localizedName substringToIndex:[localizedName length] - 4];
        }

        entry->_localizedName = localizedName;
        entry->_icon = [[NSWorkspace sharedWorkspace] iconForFile:[url path]];
    } else {
        entry->_localizedName = entry->_bundleIdentifier;
    }

    return entry;
}

+ (STZApplicationEntry *)finderEntry {
    static STZApplicationEntry *entry = nil;
    if (!entry) {
        entry = [STZApplicationEntry entryWithBundleIdentifier:@"com.apple.finder"];
    }
    return entry;
}

- (BOOL)isFinder {
    return [_bundleIdentifier isEqualToString:@"com.apple.finder"];
}

- (BOOL)isNameDefault {
    return [_localizedName isEqualToString:_bundleIdentifier];
}

- (BOOL)isEqual:(STZApplicationEntry *)other {
    if (other == self) {return YES;}
    if (![other isKindOfClass:[STZApplicationEntry self]]) {return NO;}
    return [self->_bundleIdentifier isEqualToString:other->_bundleIdentifier]
        && [self->_localizedName isEqualToString:other->_localizedName];
}

- (NSUInteger)hash {
    return [_bundleIdentifier hash] ^ [_localizedName hash];
}

- (NSComparisonResult)localizedStandardCompare:(STZApplicationEntry *)other {
    BOOL equalBundleIDs = [self->_bundleIdentifier isEqualToString:other->_bundleIdentifier];
    BOOL equalNames = [self->_localizedName isEqualToString:other->_localizedName];

    if (equalBundleIDs && equalNames) {
        return NSOrderedSame;
    }

    BOOL selfFinder = [self isFinder];
    BOOL otherFinder = [other isFinder];
    if (selfFinder != otherFinder) {
        return selfFinder ? NSOrderedAscending : NSOrderedDescending;
    }

    BOOL selfNameDefault = [self isNameDefault];
    BOOL otherNameDefault = [other isNameDefault];
    if (selfNameDefault != otherNameDefault) {
        return otherNameDefault ? NSOrderedAscending : NSOrderedDescending;
    }

    NSComparisonResult compare = [self->_localizedName localizedStandardCompare:other->_localizedName];
    if (compare != NSOrderedSame) {return compare;}
    if (!equalNames) {return [self->_localizedName compare:other->_localizedName];}
    return [self->_bundleIdentifier compare:other->_bundleIdentifier];
}

+ (void)sortEntries:(NSMutableArray<STZApplicationEntry *> *)entries {
    [entries sortUsingSelector:@selector(localizedStandardCompare:)];
}

+ (void)diffEntries:(NSArray<STZApplicationEntry *> *)old
          toEntries:(NSArray<STZApplicationEntry *> *)new
        getRemovals:(NSIndexSet **)outRemovals
      andInsertions:(NSIndexSet **)outInsertions {
    NSMutableIndexSet *removals = [NSMutableIndexSet indexSet];
    NSMutableIndexSet *insertions = [NSMutableIndexSet indexSet];

    NSInteger p = 0, oldCount = [old count];
    NSInteger q = 0, newCount = [new count];

    while (p < oldCount && q < newCount) {
        STZApplicationEntry *a = [old objectAtIndex:p];
        STZApplicationEntry *b = [new objectAtIndex:q];

        switch ([a localizedStandardCompare:b]) {
        case NSOrderedAscending:
            [removals addIndex:p];
            p += 1;
            break;
        case NSOrderedDescending:
            [insertions addIndex:q];
            q += 1;
            break;
        case NSOrderedSame:
            p += 1;
            q += 1;
            break;
        }
    }

    [removals addIndexesInRange:NSMakeRange(p, oldCount - p)];
    [insertions addIndexesInRange:NSMakeRange(q, newCount - q)];

    *outRemovals = removals;
    *outInsertions = insertions;
}

@end
