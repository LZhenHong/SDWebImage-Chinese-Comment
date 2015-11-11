/*
 * This file is part of the SDWebImage package.
 * (c) Olivier Poitrey <rs@dailymotion.com>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#import <Foundation/Foundation.h>
#import "SDWebImageDownloader.h"
#import "SDWebImageOperation.h"

// 定义通知常量
extern NSString *const SDWebImageDownloadStartNotification;
extern NSString *const SDWebImageDownloadReceiveResponseNotification;
extern NSString *const SDWebImageDownloadStopNotification;
extern NSString *const SDWebImageDownloadFinishNotification;


// SDWebImageDownloader 用的默认 operation
@interface SDWebImageDownloaderOperation : NSOperation <SDWebImageOperation>

/**
 *  下载图片的 URL 请求
 */
@property (strong, nonatomic, readonly) NSURLRequest *request;

/**
 *  是否要压缩图片
 */
@property (assign, nonatomic) BOOL shouldDecompressImages;

// ----------------------------------------不懂------------------------------------
/**
 * Whether the URL connection should consult the credential storage for authenticating the connection. `YES` by default.
 *
 * This is the value that is returned in the `NSURLConnectionDelegate` method `-connectionShouldUseCredentialStorage:`.
 */
@property (nonatomic, assign) BOOL shouldUseCredentialStorage;

/**
 * The credential used for authentication challenges in `-connection:didReceiveAuthenticationChallenge:`.
 *
 * This will be overridden by any shared credentials that exist for the username or password of the request URL, if present.
 */
@property (nonatomic, strong) NSURLCredential *credential;
// -------------------------------------------------------------------------------


/**
 *  图片下载的选项配置
 */
@property (assign, nonatomic, readonly) SDWebImageDownloaderOptions options;

/**
 *  图片的下载完成后的大小 (in bytes)
 */
@property (assign, nonatomic) NSInteger expectedSize;

/**
 *  The response returned by the operation's connection.
 *  下载图片返回的 response
 */
@property (strong, nonatomic) NSURLResponse *response;

/**
 *  初始化 SDWebImageDownloaderOperation 对象
 *
 *  @param request        图片的下载请求
 *  @param options        图片下载的选项配置
 *  @param progressBlock  下载图片过程会不断调用，当接收到数据时会调用的 block (会在后台调用的 block)
 *  @param completedBlock 图片下载完成后会调用的 block(下载成功会在主线程中调用)
 *  @param cancelBlock    取消下载图片会调用的 block
 *
 *  @return 初始化后的对象
 */
- (id)initWithRequest:(NSURLRequest *)request
              options:(SDWebImageDownloaderOptions)options
             progress:(SDWebImageDownloaderProgressBlock)progressBlock
            completed:(SDWebImageDownloaderCompletedBlock)completedBlock
            cancelled:(SDWebImageNoParamsBlock)cancelBlock;

@end
