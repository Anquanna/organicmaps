NS_ASSUME_NONNULL_BEGIN
@class MWMCarPlaySearchResultObject;

/// Called exactly once per request: with the results, or with nil when a newer CarPlay request
/// supersedes it. Requests from the phone UI share the same engine and are not detected here.
typedef void (^MWMCarPlaySearchCompletion)(NSArray<MWMCarPlaySearchResultObject *> * _Nullable searchResults);

NS_SWIFT_NAME(CarPlaySearchService)
@interface MWMCarPlaySearchService : NSObject
@property(strong, nonatomic, readonly) NSArray<MWMCarPlaySearchResultObject *> * lastResults;

- (instancetype)init;

- (void)searchText:(NSString *)text
       forInputLocale:(NSString *)inputLocale
    completionHandler:(MWMCarPlaySearchCompletion)completionHandler;
- (void)saveLastQuery;
@end

NS_ASSUME_NONNULL_END
