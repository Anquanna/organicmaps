#import "MWMCarPlaySearchService.h"
#import "MWMCarPlaySearchResultObject.h"
#import "MWMSearch.h"

#import "SwiftBridge.h"

@interface MWMCarPlaySearchService () <MWMSearchObserver>
@property(copy, nonatomic, nullable) MWMCarPlaySearchCompletion completionHandler;
@property(strong, nonatomic, nullable) NSString * lastQuery;
@property(strong, nonatomic, nullable) NSString * inputLocale;
@property(strong, nonatomic, readwrite) NSArray<MWMCarPlaySearchResultObject *> * lastResults;

@end

@implementation MWMCarPlaySearchService

- (instancetype)init
{
  self = [super init];
  if (self)
  {
    [MWMSearch addObserver:self];
    self.lastResults = @[];
  }
  return self;
}

- (void)searchText:(NSString *)text
       forInputLocale:(NSString *)inputLocale
    completionHandler:(MWMCarPlaySearchCompletion)completionHandler
{
  [self completePendingRequestWithResults:nil];

  self.lastResults = @[];
  if (text.length == 0)
  {
    [MWMSearch clear];
    completionHandler(@[]);
    return;
  }

  self.lastQuery = text;
  self.inputLocale = inputLocale;

  // MWMSearch is shared with the phone UI, which leaves the mode at viewport. A viewport search
  // never reports completion, so switch before installing the handler below: the mode change
  // restarts the previous query, and its results are not ours.
  [MWMSearch setSearchMode:SearchModeEverywhere];
  self.completionHandler = completionHandler;

  /// @todo Didn't find pure category request in CarPlay.
  SearchQuery * query = [[SearchQuery alloc] init:text locale:inputLocale source:SearchTextSourceTypedText];
  [MWMSearch searchQuery:query];
}

// Resets the handler before invoking it, since the handler may start another search of its own.
- (void)completePendingRequestWithResults:(nullable NSArray<MWMCarPlaySearchResultObject *> *)results
{
  MWMCarPlaySearchCompletion const completionHandler = self.completionHandler;
  if (completionHandler == nil)
    return;
  self.completionHandler = nil;
  completionHandler(results);
}

- (void)saveLastQuery
{
  if (self.lastQuery != nil && self.inputLocale != nil)
  {
    SearchQuery * query = [[SearchQuery alloc] init:self.lastQuery
                                             locale:self.inputLocale
                                             source:SearchTextSourceTypedText];
    [MWMSearch saveQuery:query];
  }
}

#pragma mark - MWMSearchObserver

- (void)onSearchCompleted
{
  if (self.completionHandler == nil)
    return;

  NSMutableArray<MWMCarPlaySearchResultObject *> * results = [NSMutableArray array];
  NSInteger count = [MWMSearch resultsCount];
  for (NSInteger row = 0; row < count; row++)
  {
    MWMCarPlaySearchResultObject * result = [[MWMCarPlaySearchResultObject alloc] initForRow:row];
    if (result != nil)
      [results addObject:result];
  }

  self.lastResults = results;
  [self completePendingRequestWithResults:results];
}

@end
