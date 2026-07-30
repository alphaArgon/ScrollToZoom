/*
 *  STZOptionsPanel.m
 *  ScrollToZoom
 *
 *  Created by alpha on 2025/4/16.
 *  Copyright © 2025 alphaArgon.
 */

#import "STZOptionsPanel.h"
#import "STZSettings.h"
#import "STZControls.h"
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
                                                                styleMask:NSWindowStyleMaskTitled
                                                                        | NSWindowStyleMaskClosable
                                                                        | NSWindowStyleMaskFullSizeContentView
                                                                  backing:NSBackingStoreBuffered
                                                                    defer:YES];
    [panel setTitleVisibility:NSWindowTitleHidden];
    [panel setTitlebarAppearsTransparent:YES];
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
    NSButton           *_commandBasedCheckbox;
    NSButton           *_excludingFlagsCheckBox;
    NSTextField        *_excludingFlagsLabel;
    NSButton           *_chromiumZoomFixCheckbox;
    NSTextField        *_chromiumZoomFixLabel;
    BOOL                _excludingFlagsRecommended;
    BOOL                _chromiumZoomFixRecommended;
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

    _commandBasedCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"use-command-based-zoom", nil)
                                                 target:self action:@selector(toggleCommandBased:)];
    NSTextField *commandBasedLabel = STZMessageLabel(NSLocalizedString(@"use-command-based-zoom-message", nil));

    _excludingFlagsCheckBox = [NSButton checkboxWithTitle:NSLocalizedString(@"exclude-flags", nil)
                                                   target:self action:@selector(toggleExcludingFlags:)];
    _excludingFlagsLabel = STZMessageLabel(NSLocalizedString(@"exclude-flags-message", nil));

    _chromiumZoomFixCheckbox = [NSButton checkboxWithTitle:NSLocalizedString(@"fix-chromium-zoom-stall", nil)
                                                    target:self action:@selector(toggleChromiumZoomFix:)];
    _chromiumZoomFixLabel = STZMessageLabel(NSLocalizedString(@"fix-chromium-zoom-stall-message", nil));

    NSView *panelView = [[NSView alloc] init];
    [panelView setSubviews:@[
        _enabledCheckbox,
        _commandBasedCheckbox, commandBasedLabel,
        _excludingFlagsCheckBox, _excludingFlagsLabel,
        _chromiumZoomFixCheckbox, _chromiumZoomFixLabel]];

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

    [self addSplitViewItem:listItem];
    [self addSplitViewItem:panelItem];

    [NSLayoutConstraint activateConstraints:@[
        [[_enabledCheckbox topAnchor] constraintEqualToAnchor:[panelView topAnchor] constant:kSTZUILargeSpacing],
        [[_enabledCheckbox leadingAnchor] constraintEqualToAnchor:[panelView leadingAnchor] constant:kSTZUINormalSpacing],
        [[_enabledCheckbox trailingAnchor] constraintLessThanOrEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUINormalSpacing],

        [[_commandBasedCheckbox topAnchor] constraintEqualToAnchor:[_enabledCheckbox bottomAnchor] constant:kSTZUISmallSpacing],
        [[_commandBasedCheckbox leadingAnchor] constraintEqualToAnchor:[_enabledCheckbox leadingAnchor]],
        [[_commandBasedCheckbox trailingAnchor] constraintLessThanOrEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUINormalSpacing],

        [[commandBasedLabel topAnchor] constraintEqualToAnchor:[_commandBasedCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[commandBasedLabel leadingAnchor] constraintEqualToAnchor:[_commandBasedCheckbox leadingAnchor] constant:kSTZUICheckboxWidth],
        [[commandBasedLabel trailingAnchor] constraintEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUINormalSpacing],

        [[_excludingFlagsCheckBox topAnchor] constraintEqualToAnchor:[commandBasedLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[_excludingFlagsCheckBox leadingAnchor] constraintEqualToAnchor:[_enabledCheckbox leadingAnchor]],
        [[_excludingFlagsCheckBox trailingAnchor] constraintLessThanOrEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUINormalSpacing],

        [[_excludingFlagsLabel topAnchor] constraintEqualToAnchor:[_excludingFlagsCheckBox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[_excludingFlagsLabel leadingAnchor] constraintEqualToAnchor:[_excludingFlagsCheckBox leadingAnchor] constant:kSTZUICheckboxWidth],
        [[_excludingFlagsLabel trailingAnchor] constraintEqualToAnchor:[_excludingFlagsCheckBox trailingAnchor]],
        [[_excludingFlagsLabel trailingAnchor] constraintEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUINormalSpacing],

        [[_chromiumZoomFixCheckbox topAnchor] constraintEqualToAnchor:[_excludingFlagsLabel bottomAnchor] constant:kSTZUISmallSpacing],
        [[_chromiumZoomFixCheckbox leadingAnchor] constraintEqualToAnchor:[_enabledCheckbox leadingAnchor]],
        [[_chromiumZoomFixCheckbox trailingAnchor] constraintLessThanOrEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUINormalSpacing],

        [[_chromiumZoomFixLabel topAnchor] constraintEqualToAnchor:[_chromiumZoomFixCheckbox bottomAnchor] constant:kSTZUIInlineSpacing],
        [[_chromiumZoomFixLabel leadingAnchor] constraintEqualToAnchor:[_chromiumZoomFixCheckbox leadingAnchor] constant:kSTZUICheckboxWidth],
        [[_chromiumZoomFixLabel trailingAnchor] constraintEqualToAnchor:[panelView trailingAnchor] constant:-kSTZUINormalSpacing],

        [[panelView bottomAnchor] constraintGreaterThanOrEqualToAnchor:[_chromiumZoomFixLabel bottomAnchor] constant:kSTZUINormalSpacing],
        [[panelView heightAnchor] constraintGreaterThanOrEqualToConstant:300],
        [[panelView widthAnchor] constraintGreaterThanOrEqualToConstant:250],
    ]];

    return self;
}

- (void)viewWillAppear {
    [self reloadData];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(didChangeForBundleIdentifier:)
                                                 name:(__bridge id)kSTZAppOptionsDidChangeNotification
                                               object:nil];
}

- (void)viewWillDisappear {
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:(__bridge id)kSTZAppOptionsDidChangeNotification
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

    NSDictionary *configured = (__bridge_transfer id)STZCopyOptionsForAllApps();
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
        [[cellView textField] setStringValue:NSLocalizedString(@"running-apps", nil)];
        [[cellView imageView] setImage:nil];
        [cellView setImageView:nil];

    } else if (item == _configuredBundleIDs) {
        [[cellView textField] setStringValue:NSLocalizedString(@"configured-apps", nil)];
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

    [_enabledCheckbox setTitle:[NSString stringWithFormat:NSLocalizedString(@"enabled-for-app-name-%@", nil), [entry localizedName]]];
    STZAppOptions options = STZGetAppOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier]);

    BOOL enabled = !(options & kSTZDisabledForApp);
    [_enabledCheckbox setState:enabled];
    [_commandBasedCheckbox setState:!!(options & kSTZUsesCommandBasedZoom)];
    [_commandBasedCheckbox setEnabled:enabled];
    [_excludingFlagsCheckBox setState:!!(options & kSTZFlagsExcludedForApp)];
    [_excludingFlagsCheckBox setEnabled:enabled];
    [_chromiumZoomFixCheckbox setState:!!(options & kSTZFixesZoomForChromiumApp)];
    [_chromiumZoomFixCheckbox setEnabled:enabled];

    STZAppOptions recommendedOptions = STZGetRecommendedAppOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier]);
    if (!(recommendedOptions & kSTZFlagsExcludedForApp) != !_excludingFlagsRecommended) {
        _excludingFlagsRecommended = !_excludingFlagsRecommended;
        NSString *message = NSLocalizedString(@"exclude-flags-message", nil);
        if (_excludingFlagsRecommended) {
            message = [NSString stringWithFormat:NSLocalizedString(@"%@-recommended-for-this-app", nil), message];
        }
        [_excludingFlagsLabel setStringValue:message];
    }

    if (!(recommendedOptions & kSTZFixesZoomForChromiumApp) != !_chromiumZoomFixRecommended) {
        _chromiumZoomFixRecommended = !_chromiumZoomFixRecommended;
        NSString *message = NSLocalizedString(@"fix-chromium-zoom-stall-message", nil);
        if (_chromiumZoomFixRecommended) {
            message = [NSString stringWithFormat:NSLocalizedString(@"%@-recommended-for-this-app", nil), message];
        }
        [_chromiumZoomFixLabel setStringValue:message];
    }
}

- (void)toggleEnabled:(id)sender {
    [self toggleOption:kSTZDisabledForApp byCheckbox:_enabledCheckbox flipped:YES];

    BOOL enabled = [_enabledCheckbox state] == NSControlStateValueOn;
    [_commandBasedCheckbox setEnabled:enabled];
    [_excludingFlagsCheckBox setEnabled:enabled];
    [_chromiumZoomFixCheckbox setEnabled:enabled];
}

- (void)toggleCommandBased:(id)sender {
    [self toggleOption:kSTZUsesCommandBasedZoom byCheckbox:_commandBasedCheckbox flipped:NO];
}

- (void)toggleExcludingFlags:(id)sender {
    [self toggleOption:kSTZFlagsExcludedForApp byCheckbox:_excludingFlagsCheckBox flipped:NO];
}

- (void)toggleChromiumZoomFix:(id)sender {
    [self toggleOption:kSTZFixesZoomForChromiumApp byCheckbox:_chromiumZoomFixCheckbox flipped:NO];
}

- (void)toggleOption:(STZAppOptions)option byCheckbox:(NSButton *)checkbox flipped:(BOOL)flipped {
    STZApplicationEntry *entry = [self selectedEntry];
    if (!entry) {return;}

    STZAppOptions options = STZGetAppOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier]);
    if (![checkbox state] != !flipped) {
        options |= option;
    } else {
        options &= ~option;
    }

    _changesMadeBySelf = YES;
    STZSetAppOptionsForBundleIdentifier((__bridge void *)[entry bundleIdentifier], options);
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
