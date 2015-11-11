
/*
 * This file is part of the SDWebImage package.
 * (c) Olivier Poitrey <rs@dailymotion.com>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#import "SDWebImageCompat.h"
#import "SDWebImageOperation.h"
#import "SDWebImageDownloader.h"
#import "SDImageCache.h"

typedef NS_OPTIONS(NSUInteger, SDWebImageOptions) {
    /**
     *  一个 URL 下载失败就会被加入黑名单，不再会重新下载，这个 flag 会让 SDWebImage 尝试重新下载
     */
    SDWebImageRetryFailed = 1 << 0,
    
    /**
     * 默认情况下，SDWebImage 会在交互发生时，也会下载图片
     * 这个 flag 会禁止这个特性，只有在交互完成后，才会下载图片
     * 栗子：在 scrollview 减速的时候，才会开始下载(也就是你滑动的时候 scrollview 不下载,你手从屏幕上移走, scrollview 开始减速的时候才会开始下载图片
     */
    SDWebImageLowPriority = 1 << 1,
    
    /**
     *  这个 flag 会禁止磁盘缓存，只会在内存中缓存
     */
    SDWebImageCacheMemoryOnly = 1 << 2,
    
    /**
     *  这个 flag 保证阶段性的下载，就像在浏览器里图片的显示那样，一边下载一边显示
     *  默认情况下，图片只有在下载完成后才会显示
     */
    SDWebImageProgressiveDownload = 1 << 3,
    
    /**
     *  尽管 image 缓存了，但是如果需要的话还是会从远程刷新
     *  这个 flag 解决的就是同一个 URL 对应的 image 改变了，SDWebImage 会重新下载
     *  当缓存的图片刷新之后，completion block 也会被调用一次，并且传入最终的图片
     *  这个 flag 只有在 URLs 跟缓存的图片不是一定的确认下来才会使用（图片缓存之后，这个 URL 对应的图片可能会改变）
     *  栗子：用户的头像图片
     */
    SDWebImageRefreshCached = 1 << 4,
    
    /**
     *  APP 进入后台后，还是会继续下载正在下载的图片
     */
    SDWebImageContinueInBackground = 1 << 5,
    
    /**
     *  可以控制存在 NSHTTPCookieStore 的 cookies
     */
    SDWebImageHandleCookies = 1 << 6,
    
    /**
     *  允许不安全的 SSL 证书
     */
    SDWebImageAllowInvalidSSLCertificates = 1 << 7,
    
    /**
     *  默认情况下，image 的下载是按照添加的顺序 (FIFO)
     *  这个 flag 会让 image 的下载操作移动到队列的最前端
     */
    SDWebImageHighPriority = 1 << 8,
    
    /**
     *  默认情况，占位图片会在现在图片的时候显示
     *  这个 flag 会延迟占位图片的显示（等到图片下载完成后才显示占位图）
     */
    SDWebImageDelayPlaceholder = 1 << 9,
    
    /**
     *  是否 transform 图片 (是否是图片有可能方向不对需要调整方向，例如采用iPhone拍摄的照片如果不纠正方向，那么图片是向左旋转90度的。可能很多人不知道iPhone的摄像头并不是竖直的,而是向左偏了90度)
     */
    SDWebImageTransformAnimatedImage = 1 << 10,
    
    /**
     *  控制是否自动设置图片
     */
    SDWebImageAvoidAutoSetImage = 1 << 11
};

/**
 *  当获取到图片之后的回调 block
 *
 *  @param image     获得的图片
 *  @param error     获取图片出错
 *  @param cacheType 图片获取的方式 ( memory(内存) \ disk(磁盘) \ None(从网络中下载) )
 *  @param imageURL  图片的 URL 路径
 */
typedef void(^SDWebImageCompletionBlock)(UIImage *image, NSError *error, SDImageCacheType cacheType, NSURL *imageURL);

/**
 *  与上个 block 没有太多的区别，多了一个判断是否完成的参数 finished
 */
typedef void(^SDWebImageCompletionWithFinishedBlock)(UIImage *image, NSError *error, SDImageCacheType cacheType, BOOL finished, NSURL *imageURL);

typedef NSString *(^SDWebImageCacheKeyFilterBlock)(NSURL *url);



@class SDWebImageManager;

@protocol SDWebImageManagerDelegate <NSObject>
@optional
/**
 *  代理方法：主要作用是当缓存里没有发现某张图片的缓存时，是否选择下载这张图片(默认是 YES)，可以选择 NO，那么 SDWebImage 在缓存中没有找到这张图片的时候不会选择下载
 *
 *  @param imageManager 当前对应的 SDWebImageManager
 *  @param imageURL     图片对应的 URL
 *
 *  @return 是否下载图片
 */
- (BOOL)imageManager:(SDWebImageManager *)imageManager shouldDownloadImageForURL:(NSURL *)imageURL;

/**
 *  下载完成后的图片在没有加入缓存(memory \ disk)之前就 transform 这个图片，在异步线程中执行，防止阻塞主线程
 *  图片的 transform 是很耗资源
 *
 *  @param imageManager 当前对应的 imageManager
 *  @param image        要 transform 的图片
 *  @param imageURL     图片对应的 URL
 *
 *  @return transform 后的图片
 */
- (UIImage *)imageManager:(SDWebImageManager *)imageManager transformDownloadedImage:(UIImage *)image withURL:(NSURL *)imageURL;

@end



@interface SDWebImageManager : NSObject

@property (weak, nonatomic) id <SDWebImageManagerDelegate> delegate;

/**
 *  SDWebImageManager 会绑定一个异步下载器(downloader)和一个缓存(cache)
 */
@property (strong, nonatomic, readonly) SDImageCache *imageCache;
@property (strong, nonatomic, readonly) SDWebImageDownloader *imageDownloader;

/**
 *  在每次将 URL 转换成 cache key，会调用这个 filter 对图片的 URL 进行操作
 */
@property (nonatomic, copy) SDWebImageCacheKeyFilterBlock cacheKeyFilter;

+ (SDWebImageManager *)sharedManager;

/**
 *  下载缓存中没有的图片或者返回从缓存中取出来的图片
 *
 *  @param url            图片的 URL
 *  @param options        这个请求的 Option
 *  @param progressBlock  在图片下载的时候会调用的 block
 *  @param completedBlock 在图片下载完成后会调用的 block
 *
 *  @return 返回值是一个遵守 SDWebImageOperation 协议的 NSObject 类
 */
- (id <SDWebImageOperation>)downloadImageWithURL:(NSURL *)url
                                         options:(SDWebImageOptions)options
                                        progress:(SDWebImageDownloaderProgressBlock)progressBlock
                                       completed:(SDWebImageCompletionWithFinishedBlock)completedBlock;

/**
 *  为给定的 URL 存储图片到缓存中
 *
 *  @param image 要缓存的图片
 *  @param url   缓存图片的 URL
 */
- (void)saveImageToCache:(UIImage *)image forURL:(NSURL *)url;

/**
 *  取消当前所有正在下载图片的 operation
 */
- (void)cancelAll;

/**
 *  检查是否有 operation 在执行，即是否有图片在下载
 */
- (BOOL)isRunning;

/**
 *  检查图片是否已经被缓存
 *
 *  @param url 图片的 URL
 */
- (BOOL)cachedImageExistsForURL:(NSURL *)url;

/**
 *  检查图片是否已经被缓存到磁盘 (仅存在在 disk)
 *
 *  @param url 图片的 URL
 */
- (BOOL)diskImageExistsForURL:(NSURL *)url;

/**
 *  检查图片是否已经被缓存
 *  这里的 completion block 在主线程中调用
 *
 *  @param url             图片的 URL
 *  @param completionBlock 当检查完成后要被调用的 block
 */
- (void)cachedImageExistsForURL:(NSURL *)url
                     completion:(SDWebImageCheckCacheCompletionBlock)completionBlock;

/**
 *  检查图片是否已经被缓存到磁盘 (仅存在在 disk)
 *  这里的 completion block 在主线程中调用
 *
 *  @param url             图片的 URL
 *  @param completionBlock 当检查完成后要被调用的 block
 */
- (void)diskImageExistsForURL:(NSURL *)url
                   completion:(SDWebImageCheckCacheCompletionBlock)completionBlock;

/**
 *  用图片的 URL 获得图片的 cache key
 *
 *  @param url 图片的 URL
 *
 *  @return 图片的 cache key
 */
- (NSString *)cacheKeyForURL:(NSURL *)url;

@end
