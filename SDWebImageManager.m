/*
 * This file is part of the SDWebImage package.
 * (c) Olivier Poitrey <rs@dailymotion.com>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#import "SDWebImageManager.h"
#import <objc/message.h>

@interface SDWebImageCombinedOperation : NSObject <SDWebImageOperation>

@property (assign, nonatomic, getter = isCancelled) BOOL cancelled;
@property (copy, nonatomic) SDWebImageNoParamsBlock cancelBlock;
@property (strong, nonatomic) NSOperation *cacheOperation;

@end

@interface SDWebImageManager ()

@property (strong, nonatomic, readwrite) SDImageCache *imageCache;
@property (strong, nonatomic, readwrite) SDWebImageDownloader *imageDownloader;
// set 的访问速度比 array 快
// NSSet 类有 hash 特性，访问的速度比 NSArray 快的多
@property (strong, nonatomic) NSMutableSet *failedURLs;
@property (strong, nonatomic) NSMutableArray *runningOperations;

@end

@implementation SDWebImageManager

/**
 *  一个 SDWebImageManager 管理所有图片的下载
 */
+ (id)sharedManager {
    static dispatch_once_t once;
    static id instance;
    dispatch_once(&once, ^{
        instance = [self new];
    });
    return instance;
}

/**
 *  初始化方法，对属性进行初始化
 *  生成 SDImageCache 单例，获得 SDWebImageDownloader 单例，初始化一个 NSMutableSet 用来存储下载失败图片的 URL
 *  创建可变数组存储下载 operation
 */
- (id)init {
    if ((self = [super init])) {
        _imageCache = [self createCache];
        _imageDownloader = [SDWebImageDownloader sharedDownloader];
        _failedURLs = [NSMutableSet new];
        _runningOperations = [NSMutableArray new];
    }
    return self;
}

- (SDImageCache *)createCache {
    return [SDImageCache sharedImageCache];
}

/**
 *  用图片的 URL 来生成对应的 cache key
 *  当 block cacheKeyFilter 不为空时，利用 cacheKeyFilter 来处理 URL 生成 cache key
 *  如果为空直接返回 URL 的 absoluteString 内容
 *
 *  @param url 图片 URL
 *
 *  @return 图片的 cache key
 */
- (NSString *)cacheKeyForURL:(NSURL *)url {
    if (self.cacheKeyFilter) {
        return self.cacheKeyFilter(url);
    }
    else {
        return [url absoluteString];
    }
}

/**
 *  用图片的 URL 来检查是否已被缓存
 *  先从 memory 中查找，没有再从 disk 中查找
 *
 *  @param url 图片 URL
 */
- (BOOL)cachedImageExistsForURL:(NSURL *)url {
    NSString *key = [self cacheKeyForURL:url];
    if ([self.imageCache imageFromMemoryCacheForKey:key] != nil) return YES;
    // SDImageCache 的方法
    return [self.imageCache diskImageExistsWithKey:key];
}

/**
 *  从 disk 中查找是否有 URL 对应的缓存
 */
- (BOOL)diskImageExistsForURL:(NSURL *)url {
    NSString *key = [self cacheKeyForURL:url];
    return [self.imageCache diskImageExistsWithKey:key];
}

/**
 *  检查图片是否已经被缓存，先检查内存缓存，在检查 disk 缓存
 *  这里的 completion block 在主线程中调用，block 接收是否缓存参数
 *
 *  typedef void(^SDWebImageCheckCacheCompletionBlock)(BOOL isInCache); // 定义在 SDImageCache
 *  @param completionBlock 在主线程中调用，block 接收是否缓存参数
 */
- (void)cachedImageExistsForURL:(NSURL *)url
                     completion:(SDWebImageCheckCacheCompletionBlock)completionBlock {
    NSString *key = [self cacheKeyForURL:url];

    // 检查内存中是否有缓存
    BOOL isInMemoryCache = ([self.imageCache imageFromMemoryCacheForKey:key] != nil);

    if (isInMemoryCache) {
        // making sure we call the completion block on the main queue
        // 主线程回调 block
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completionBlock) {
                completionBlock(YES);
            }
        });
        return;
    }

    // 在异步线程检查图片缓存到 disk 中
    [self.imageCache diskImageExistsWithKey:key completion:^(BOOL isInDiskCache) {
        // the completion block of checkDiskCacheForImageWithKey:completion: is always called on the main queue, no need to further dispatch
        // 方法 checkDiskCacheForImageWithKey:completion: 的 completion block 会在主线程中调用
        if (completionBlock) {
            completionBlock(isInDiskCache);
        }
    }];
}

/**
 *  检查图片是否存在在 disk 中
 *
 *  typedef void(^SDWebImageCheckCacheCompletionBlock)(BOOL isInCache); // 定义在 SDImageCache
 *  @param completionBlock 在主线程中调用，block 接收是否缓存参数
 */
- (void)diskImageExistsForURL:(NSURL *)url
                   completion:(SDWebImageCheckCacheCompletionBlock)completionBlock {
    NSString *key = [self cacheKeyForURL:url];

    [self.imageCache diskImageExistsWithKey:key completion:^(BOOL isInDiskCache) {
        // the completion block of checkDiskCacheForImageWithKey:completion: is always called on the main queue, no need to further dispatch
        if (completionBlock) {
            completionBlock(isInDiskCache);
        }
    }];
}


/**
 *  Most Important
 *  利用图片的 URL 生成给一个 operation 来下载图片
 *
 *  @param url            图片的 URL
 *  @param options        传入一个 flag 来控制图片的操作
 *  @param progressBlock  在图片下载的时候会调用的 block
 *  @param completedBlock 在图片下载完成后会调用的 block
 *
 *  @return 返回值是一个遵守 SDWebImageOperation 协议的 NSObject 类
 */
- (id <SDWebImageOperation>)downloadImageWithURL:(NSURL *)url
                                         options:(SDWebImageOptions)options
                                        progress:(SDWebImageDownloaderProgressBlock)progressBlock
                                       completed:(SDWebImageCompletionWithFinishedBlock)completedBlock {
    // Invoking this method without a completedBlock is pointless
    // 保证 completion block 有值
    NSAssert(completedBlock != nil, @"If you mean to prefetch the image, use -[SDWebImagePrefetcher prefetchURLs] instead");

    // Very common mistake is to send the URL using NSString object instead of NSURL. For some strange reason, Xcode won't
    // throw any warning for this type mismatch. Here we failsafe this error by allowing URLs to be passed as NSString.
    // 用户可能传进来的是 NSString，而不是 NSURL，这里对 NSString 进行处理，防止 crash
    if ([url isKindOfClass:NSString.class]) {
        url = [NSURL URLWithString:(NSString *)url];
    }

    // Prevents app crashing on argument type error like sending NSNull instead of NSURL
    // 如果 url 不是 NSURL 类型的，将 url 设置为 nil，避免因为参数类型问题而导致 crash
    if (![url isKindOfClass:NSURL.class]) {
        url = nil;
    }

    //
    __block SDWebImageCombinedOperation *operation = [SDWebImageCombinedOperation new];
    __weak SDWebImageCombinedOperation *weakOperation = operation;

    BOOL isFailedUrl = NO;
    // 创建互斥锁防止其他线程修改 failedURLs 数组
    // 判断这个 URL 是否下载失败过
    @synchronized (self.failedURLs) {
        isFailedUrl = [self.failedURLs containsObject:url];
    }

    // URL 的 string 长度为零，如果 options 中没有 SDWebImageRetryFailed 选项，或者是下载失败了的 URL，抛出 error
    // 当 options contain SDWebImageRetryFailed 条件判断就为 NO
    if (url.absoluteString.length == 0 || (!(options & SDWebImageRetryFailed) && isFailedUrl)) {
        dispatch_main_sync_safe(^{
            NSError *error = [NSError errorWithDomain:NSURLErrorDomain code:NSURLErrorFileDoesNotExist userInfo:nil];
            completedBlock(nil, error, SDImageCacheTypeNone, YES, url);
        });
        return operation;
    }

    // 创建互斥锁避免其他的线程修改 runningOperations
    @synchronized (self.runningOperations) {
        // 将图片的下载线程添加到执行中的 runningOperations 数组中
        [self.runningOperations addObject:operation];
    }
    NSString *key = [self cacheKeyForURL:url];

    // 从缓存中查找图片
    // cacheOperation 是用来在 disk 中异步查找图片的 operation ?
    // TODO: cacheOperation 是用来下载图片并且缓存的 operation ?
    operation.cacheOperation = [self.imageCache queryDiskCacheForKey:key done:^(UIImage *image, SDImageCacheType cacheType) {
        // 判断 operation 是否被取消，如果被取消，就从 runningOperations 数组中删除，并且 return
        if (operation.isCancelled) {
            @synchronized (self.runningOperations) {
                [self.runningOperations removeObject:operation];
            }

            return;
        }

        if ((!image || options & SDWebImageRefreshCached) && (![self.delegate respondsToSelector:@selector(imageManager:shouldDownloadImageForURL:)] || [self.delegate imageManager:self shouldDownloadImageForURL:url])) {
            if (image && options & SDWebImageRefreshCached) {
                dispatch_main_sync_safe(^{
                    // If image was found in the cache but SDWebImageRefreshCached is provided, notify about the cached image
                    // AND try to re-download it in order to let a chance to NSURLCache to refresh it from server.
                    completedBlock(image, nil, cacheType, YES, url);
                });
            }

            // download if no image or requested to refresh anyway, and download allowed by delegate
            SDWebImageDownloaderOptions downloaderOptions = 0;
            if (options & SDWebImageLowPriority) downloaderOptions |= SDWebImageDownloaderLowPriority;
            if (options & SDWebImageProgressiveDownload) downloaderOptions |= SDWebImageDownloaderProgressiveDownload;
            if (options & SDWebImageRefreshCached) downloaderOptions |= SDWebImageDownloaderUseNSURLCache;
            if (options & SDWebImageContinueInBackground) downloaderOptions |= SDWebImageDownloaderContinueInBackground;
            if (options & SDWebImageHandleCookies) downloaderOptions |= SDWebImageDownloaderHandleCookies;
            if (options & SDWebImageAllowInvalidSSLCertificates) downloaderOptions |= SDWebImageDownloaderAllowInvalidSSLCertificates;
            if (options & SDWebImageHighPriority) downloaderOptions |= SDWebImageDownloaderHighPriority;
            if (image && options & SDWebImageRefreshCached) {
                // force progressive off if image already cached but forced refreshing
                downloaderOptions &= ~SDWebImageDownloaderProgressiveDownload;
                // ignore image read from NSURLCache if image if cached but force refreshing
                downloaderOptions |= SDWebImageDownloaderIgnoreCachedResponse;
            }
            
            // 调用 SDWebImageDownloader 从网络中加载图片，并将下载 operation 返回
            id <SDWebImageOperation> subOperation = [self.imageDownloader downloadImageWithURL:url options:downloaderOptions progress:progressBlock completed:^(UIImage *downloadedImage, NSData *data, NSError *error, BOOL finished) {
                if (weakOperation.isCancelled) {
                    // TODO: Go github and see
                    // Do nothing if the operation was cancelled
                    // See #699 for more details
                    // if we would call the completedBlock, there could be a race condition between this block and another completedBlock for the same object, so if this one is called second, we will overwrite the new data
                }
                else if (error) { // 下载失败
                    dispatch_main_sync_safe(^{
                        if (!weakOperation.isCancelled) {
                            completedBlock(nil, error, SDImageCacheTypeNone, finished, url);
                        }
                    });
                    
                    // 判断错误的原因，如果错误的原因不是网络的问题，就将这个 URL 添加到 failedURLs 数组中
                    if (   error.code != NSURLErrorNotConnectedToInternet
                        && error.code != NSURLErrorCancelled
                        && error.code != NSURLErrorTimedOut
                        && error.code != NSURLErrorInternationalRoamingOff
                        && error.code != NSURLErrorDataNotAllowed
                        && error.code != NSURLErrorCannotFindHost
                        && error.code != NSURLErrorCannotConnectToHost) {
                        @synchronized (self.failedURLs) {
                            [self.failedURLs addObject:url];
                        }
                    }
                } // error
                else {
                    // 如果 options 中有 retry 这个 flag，就将 URL 从 failedURLs 移除
                    if ((options & SDWebImageRetryFailed)) {
                        @synchronized (self.failedURLs) {
                            [self.failedURLs removeObject:url];
                        }
                    }

                    // 确认是否要缓存到 disk 中
                    BOOL cacheOnDisk = !(options & SDWebImageCacheMemoryOnly);

                    if (options & SDWebImageRefreshCached && image && !downloadedImage) {
                        // Image refresh hit the NSURLCache cache, do not call the completion block
                    }
                    // 下载好的 image 要 transform
                    else if (downloadedImage && (!downloadedImage.images || (options & SDWebImageTransformAnimatedImage)) && [self.delegate respondsToSelector:@selector(imageManager:transformDownloadedImage:withURL:)]) {
                        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                            // 异步线程执行图片的 transform
                            UIImage *transformedImage = [self.delegate imageManager:self transformDownloadedImage:downloadedImage withURL:url];

                            if (transformedImage && finished) {
                                // UIImage 有 isEqual: 方法判断两张图片是否相等
                                BOOL imageWasTransformed = ![transformedImage isEqual:downloadedImage];
                                // 将图片缓存
                                [self.imageCache storeImage:transformedImage recalculateFromImage:imageWasTransformed imageData:(imageWasTransformed ? nil : data) forKey:key toDisk:cacheOnDisk];
                            }

                            dispatch_main_sync_safe(^{
                                if (!weakOperation.isCancelled) {
                                    completedBlock(transformedImage, nil, SDImageCacheTypeNone, finished, url);
                                }
                            });
                        });
                    }
                    else {
                        if (downloadedImage && finished) {
                            [self.imageCache storeImage:downloadedImage recalculateFromImage:NO imageData:data forKey:key toDisk:cacheOnDisk];
                        }

                        dispatch_main_sync_safe(^{
                            if (!weakOperation.isCancelled) {
                                completedBlock(downloadedImage, nil, SDImageCacheTypeNone, finished, url);
                            }
                        });
                    }
                }

                if (finished) {
                    // 完成之后，从 runningOperations 移除这个下载 operation
                    @synchronized (self.runningOperations) {
                        [self.runningOperations removeObject:operation];
                    }
                }
            }]; // self.imageDownloader downloaderImageWithURL:...
            
            // 设置 SDWebImageCombinedOperation 的 cancelBlock，这个类的设计就是为了代码的简洁
            // 当 [SDWebImageCombinedOperation cancel] 后，会调用 cancelBlock
            operation.cancelBlock = ^{
                [subOperation cancel];

                @synchronized (self.runningOperations) {
                    [self.runningOperations removeObject:weakOperation];
                }
            };
        } // if ((!image || options & SDWebImageRefreshCached) && (![self.delegate respondsToSelector:@selector(imageManager:shouldDownloadImageForURL:)] || [self.delegate imageManager:self shouldDownloadImageForURL:url]))
        else if (image) { // 从 disk 中查询到 image
            dispatch_main_sync_safe(^{
                if (!weakOperation.isCancelled) {
                    completedBlock(image, nil, cacheType, YES, url);
                }
            });
            @synchronized (self.runningOperations) {
                [self.runningOperations removeObject:operation];
            }
        }
        else {
            // Image not in cache and download disallowed by delegate
            // image 即不存在于 cache 中，delegate 又不允许下载
            dispatch_main_sync_safe(^{
                if (!weakOperation.isCancelled) {
                    completedBlock(nil, nil, SDImageCacheTypeNone, YES, url);
                }
            });
            @synchronized (self.runningOperations) {
                [self.runningOperations removeObject:operation];
            }
        }
    }]; // self.imageCache queryDiskCacheForKey:...

    return operation;
}


/**
 *  将 image 存储到 cache 中
 *
 *  @param image 要存储的图片
 *  @param url   图片 URL
 */
- (void)saveImageToCache:(UIImage *)image forURL:(NSURL *)url {
    if (image && url) {
        NSString *key = [self cacheKeyForURL:url];
        [self.imageCache storeImage:image forKey:key toDisk:YES];
    }
}

/**
 *  取消所有正在下载的 operation
 */
- (void)cancelAll {
    @synchronized (self.runningOperations) {
        NSArray *copiedOperations = [self.runningOperations copy];
        // makeObjectsPerformSelector:
        [copiedOperations makeObjectsPerformSelector:@selector(cancel)];
        [self.runningOperations removeObjectsInArray:copiedOperations];
    }
}

/**
 *  判断是否有图片正在下载
 */
- (BOOL)isRunning {
    return self.runningOperations.count > 0;
}

@end


@implementation SDWebImageCombinedOperation

- (void)setCancelBlock:(SDWebImageNoParamsBlock)cancelBlock {
    // check if the operation is already cancelled, then we just call the cancelBlock
    if (self.isCancelled) {
        if (cancelBlock) {
            cancelBlock();
        }
        _cancelBlock = nil; // don't forget to nil the cancelBlock, otherwise we will get crashes
    } else {
        _cancelBlock = [cancelBlock copy];
    }
}

- (void)cancel {
    // 这里取消了查询 operation，下载 operation，并且将自己从 runningOperations 移除
    self.cancelled = YES;
    if (self.cacheOperation) {
        [self.cacheOperation cancel];
        self.cacheOperation = nil;
    }
    if (self.cancelBlock) {
        self.cancelBlock();

        // TODO: this is a temporary fix to #809.
        // Until we can figure the exact cause of the crash, going with the ivar instead of the setter
//        self.cancelBlock = nil;
        _cancelBlock = nil;
    }
}

@end
