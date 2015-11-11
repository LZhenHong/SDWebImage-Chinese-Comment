/*
 * This file is part of the SDWebImage package.
 * (c) Olivier Poitrey <rs@dailymotion.com>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#import <Foundation/Foundation.h>
#import "SDWebImageCompat.h"
#import "SDWebImageOperation.h"

typedef NS_OPTIONS(NSUInteger, SDWebImageDownloaderOptions) {
    // 图片的下载在较低的优先级队列
    SDWebImageDownloaderLowPriority = 1 << 0,
    
    // 图片支持阶段性的下载
    SDWebImageDownloaderProgressiveDownload = 1 << 1,
    
    // 默认情况下，请求会停用 NSURLCache，这个 flag 就是让 NSURLCache 使用默认的策略
    SDWebImageDownloaderUseNSURLCache = 1 << 2,
    
    // 如果图片是从 NSURLCache 获得，则 completion block 的 image/imageData 参数会是 nil
    // 需要与 SDWebImageDownloaderUseNSURLCache 一起使用
    SDWebImageDownloaderIgnoreCachedResponse = 1 << 3,
    
    // 在 iOS 4+ 系统中，当应用进入后台继续下载图片
    // 该操作通过向系统申请额外的时间来完成后台下载，如果后台任务终止，则操作会被取消
    SDWebImageDownloaderContinueInBackground = 1 << 4,
    
    // 将 NSMutableURLRequest.HTTPShouldHandleCookies = YES，来处理存储在 NSHTTPCookieStore 的cookie
    SDWebImageDownloaderHandleCookies = 1 << 5,
    
    // 允许不受信任的 SSL 的证书，主要用于测试
    SDWebImageDownloaderAllowInvalidSSLCertificates = 1 << 6,
    
    // 将图片的下载放在优先级较高的队列中
    SDWebImageDownloaderHighPriority = 1 << 7,
};

typedef NS_ENUM(NSInteger, SDWebImageDownloaderExecutionOrder) {
    // 默认值，所有的下载的 operation 会按照 FIFO 的顺序执行（队列方式）
    SDWebImageDownloaderFIFOExecutionOrder,
    // 所有的下载 operation 按照 FILO 的顺序执行（栈方式）
    SDWebImageDownloaderLIFOExecutionOrder
};

// 下载开始的通知
extern NSString *const SDWebImageDownloadStartNotification;
// 下载停止的通知
extern NSString *const SDWebImageDownloadStopNotification;

/**
 *  在下载过程中调用的 block
 *
 *  @param receivedSize 收到的图片二进制流大小
 *  @param expectedSize 原本的图片大小
 */
typedef void(^SDWebImageDownloaderProgressBlock)(NSInteger receivedSize, NSInteger expectedSize);

/**
 *  下载完成后会调用的 block
 *
 *  @param image    下载完后的 image
 *  @param data     图片二进制流数据
 *  @param error    下载过程中的可能的错误
 *  @param finished 是否下载完成
 */
typedef void(^SDWebImageDownloaderCompletedBlock)(UIImage *image, NSData *data, NSError *error, BOOL finished);

// TODO: More details
/**
 *  Header 过滤
 *
 *  @param url     <#url description#>
 *  @param headers <#headers description#>
 *
 *  @return <#return value description#>
 */
typedef NSDictionary *(^SDWebImageDownloaderHeadersFilterBlock)(NSURL *url, NSDictionary *headers);


/**
 *  异步下载多张图片
 */
@interface SDWebImageDownloader : NSObject
// 是否压缩图片，压缩下载完成的图片会提升体验，但是会消耗大量内存
// 默认是 YES，当你因为内存消耗而 crash 时，将这个属性设为 NO
@property (assign, nonatomic) BOOL shouldDecompressImages;

// 最多的并发数（最多能同时下载多少张图片）
@property (assign, nonatomic) NSInteger maxConcurrentDownloads;

// 当前正在下载图片的数量
@property (readonly, nonatomic) NSUInteger currentDownloadCount;

// 下载图片的 timeout(in seconds)，默认是 15.0s
@property (assign, nonatomic) NSTimeInterval downloadTimeout;

// 图片下载顺序(FIFO/FILO)
@property (assign, nonatomic) SDWebImageDownloaderExecutionOrder executionOrder;

// SDWebImageDownloader 单例对象
+ (SDWebImageDownloader *)sharedDownloader;

// 用户名属性
@property (strong, nonatomic) NSString *username;

// 密码属性
@property (strong, nonatomic) NSString *password;

/**
 *  More to go!!!
 */
@property (nonatomic, copy) SDWebImageDownloaderHeadersFilterBlock headersFilter;

/**
 *  设置拼接在 HTTP 请求后的参数值
 *
 *  @param value 参数值，当要删除 header 时，设置值为 nil
 *  @param field 需要的参数
 */
- (void)setValue:(NSString *)value forHTTPHeaderField:(NSString *)field;

/**
 *  获取特定 header field 的值
 *
 *  @return 返回与 field 相关联的 value，没有匹配的 field 就返回 nil
 */
- (NSString *)valueForHTTPHeaderField:(NSString *)field;

/**
 *  当要创建一个 operation 下载图片时，就会使用 operationClass 创建，默认是 SDWebImageDownloderOperation
 *
 *  @param operationClass 要返回 SDWebImageDownloderOperation 时，设置 operationClass 为 nil
 */
- (void)setOperationClass:(Class)operationClass;

/** Most Important!!!
 *  用给定的 URL 创建一个异步下载 downloder SDWebImageDownloder
 *
 *  @param url            图片的 URL
 *  @param options        下载图片的用的 options
 *  @param progressBlock  在下载过程重复调用的 block
 *  @param completedBlock 下载完成后调用的 block
 *  If the download succeeded, the image parameter is set, in case of error,
 *                       error parameter is set with the error. The last parameter is always YES
 *                       if SDWebImageDownloaderProgressiveDownload isn't use. With the
 *                       SDWebImageDownloaderProgressiveDownload option, this block is called
 *                       repeatedly with the partial image object and the finished argument set to NO
 *                       before to be called a last time with the full image and finished argument
 *                       set to YES. In case of error, the finished argument is always YES.
 *
 *  @return 可以取消的 SDWebImageOperation
 */
- (id <SDWebImageOperation>)downloadImageWithURL:(NSURL *)url
                                         options:(SDWebImageDownloaderOptions)options
                                        progress:(SDWebImageDownloaderProgressBlock)progressBlock
                                       completed:(SDWebImageDownloaderCompletedBlock)completedBlock;
/**
 *  设置下载队列的挂起状态
 */
- (void)setSuspended:(BOOL)suspended;

@end
