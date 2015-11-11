/*
 * This file is part of the SDWebImage package.
 * (c) Olivier Poitrey <rs@dailymotion.com>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 */

#import <Foundation/Foundation.h>
#import "SDWebImageCompat.h"

typedef NS_ENUM(NSInteger, SDImageCacheType) {
    /**
     *  查询的图片不是在缓存中拿到的，而是从网络上下载的
     */
    SDImageCacheTypeNone,
    
    /**
     *  查询的图片从 disk 中获取
     */
    SDImageCacheTypeDisk,
    
    /**
     *  查询的图片从 memory 中获取的
     */
    SDImageCacheTypeMemory
};

/**
 *  查询完成后的回调 block
 *
 *  @param image     获取到的图片
 *  @param cacheType 获取图片的方式
 */
typedef void(^SDWebImageQueryCompletedBlock)(UIImage *image, SDImageCacheType cacheType);

/**
 *  查询是否完成缓存之后调用的 block
 *
 *  @param isInCache 是否缓存完毕
 */
typedef void(^SDWebImageCheckCacheCompletionBlock)(BOOL isInCache);

/**
 *  计算 disk 缓存中的总大小
 *
 *  @param fileCount 文件数量
 *  @param totalSize 文件总大小
 */
typedef void(^SDWebImageCalculateSizeBlock)(NSUInteger fileCount, NSUInteger totalSize);



/**
 *  SDImageCache 有一个 memory cache 和一个可选的 disk cache
 *  disk cache 的写操作是异步执行不会阻塞主线程
 */
@interface SDImageCache : NSObject

/**
 *  是否压缩图片，默认是 YES，压缩图片会提高体验，但是会消耗大量的内存
 *  遇到因为消耗大量内存而 crash，将这个属性设为 NO
 */
@property (assign, nonatomic) BOOL shouldDecompressImages;

/**
 *  默认是 YES
 */
@property (assign, nonatomic) BOOL shouldDisableiCloud;

/**
 *  是否在内存中缓存图片，默认 YES
 */
@property (assign, nonatomic) BOOL shouldCacheImagesInMemory;

/**
 *  内存缓存占用最大的空间，cost 像素值
 */
@property (assign, nonatomic) NSUInteger maxMemoryCost;

/**
 *  内存缓存保持的最多数量的图片
 */
@property (assign, nonatomic) NSUInteger maxMemoryCountLimit;

/**
 *  图片在缓存中保持最久的时间，按秒计算
 */
@property (assign, nonatomic) NSInteger maxCacheAge;

/**
 *  缓存的最大容量，按 byte 计算
 */
@property (assign, nonatomic) NSUInteger maxCacheSize;


/**
 *  获得 SDImageCache 单例
 */
+ (SDImageCache *)sharedImageCache;

/**
 *  用一个命名空间来生成一个 SDImageCache
 *
 *  @param ns 命名空间
 */
- (id)initWithNamespace:(NSString *)ns;

/**
 *  用指定的路径来初始化一个有命名空间的 SDImageCache
 *
 *  @param ns        命名空间
 *  @param directory disk 缓存的路径
 */
- (id)initWithNamespace:(NSString *)ns diskCacheDirectory:(NSString *)directory;

/**
 *  用 namespace 为缓存生成的路径
 *
 *  @param fullNamespace 要拼接的命名空间
 *
 *  @return 缓存的路径
 */
- (NSString *)makeDiskCachePath:(NSString *)fullNamespace;

/**
 *  这个只在你提前为你的 APP 打包好了图片才有用
 *  添加一个提前缓存好图片的路径
 *
 *  @param path 只读 cache 的路径
 */
- (void)addReadOnlyCachePath:(NSString *)path;

/**
 *  用 key 将图片缓存到 memory 和 disk 中
 *
 *  @param image 要缓存的图片
 *  @param key   要缓存的图片的键(key)
 */
- (void)storeImage:(UIImage *)image forKey:(NSString *)key;

/**
 *  有一个参数控制是否将图片缓存到 disk 中
 *
 *  @param toDisk 控制是否要缓存到 disk
 */
- (void)storeImage:(UIImage *)image forKey:(NSString *)key toDisk:(BOOL)toDisk;

// TODO: The function of parameter --recalculate--
/**
 *  将图片添加到 memory 缓存，disk 缓存可选
 *
 *  @param image       要缓存的图片
 *  @param recalculate <#recalculate description#>
 *  @param imageData   从远程服务器返回的数据，用来缓存的二进制数据，不是将图片转化成能缓存的数据，直接缓存这个会节省 CPU 性能
 *  @param key         缓存图片的 key
 *  @param toDisk      是否缓存到 disk 中
 */
- (void)storeImage:(UIImage *)image recalculateFromImage:(BOOL)recalculate imageData:(NSData *)imageData forKey:(NSString *)key toDisk:(BOOL)toDisk;

/**
 *  用一个 key 异步查询 disk 缓存
 *
 *  @param key       要查询图片的 key
 *  @param doneBlock 查询完成之后回调的 block
 *
 *  @return 异步查询图片的 operation，返回之后可以在外部对其进行取消等操作
 */
- (NSOperation *)queryDiskCacheForKey:(NSString *)key done:(SDWebImageQueryCompletedBlock)doneBlock;

/**
 *  异步查询 memory 缓存中的图片
 */
- (UIImage *)imageFromMemoryCacheForKey:(NSString *)key;

/**
 *  异步查询 disk 缓存中的图片
 */
- (UIImage *)imageFromDiskCacheForKey:(NSString *)key;

/**
 *  (串行) 移除 memory 和 disk 中缓存
 */
- (void)removeImageForKey:(NSString *)key;

/**
 *  (串行) 移除 memory 和 disk 中缓存，完成之后会回调 completion block
 *
 *  @param completion 完成移除之后要回调的 block
 */
- (void)removeImageForKey:(NSString *)key withCompletion:(SDWebImageNoParamsBlock)completion;

/**
 *  移除 memory 中的缓存，移除 disk 中的缓存是可选的
 *
 *  @param fromDisk 是否移除 disk 中的缓存
 */
- (void)removeImageForKey:(NSString *)key fromDisk:(BOOL)fromDisk;

/**
 *  移除 memory 中的缓存，移除 disk 中的缓存是可选的，完成移除会调用 completion block
 *
 *  @param fromDisk   是否移除 disk 中的缓存
 *  @param completion 完成移除后回调的 block
 */
- (void)removeImageForKey:(NSString *)key fromDisk:(BOOL)fromDisk withCompletion:(SDWebImageNoParamsBlock)completion;

/**
 *  删除所有的 memory 缓存图片
 */
- (void)clearMemory;

/**
 *  删除所有 disk 缓存图片，但是这这个方法会马上返回
 *
 *  @param completion 删除所有的图片之后会调用的 block
 */
- (void)clearDiskOnCompletion:(SDWebImageNoParamsBlock)completion;

/**
 *  删除所有的 disk 缓存图片
 */
- (void)clearDisk;

/**
 *  删除所有过期的 disk 缓存图片，马上返回
 *
 *  @param completionBlock 删除所有过期的图片之后会调用的 block
 */
- (void)cleanDiskWithCompletionBlock:(SDWebImageNoParamsBlock)completionBlock;

/**
 *  删除所有过期的 disk 缓存图片
 */
- (void)cleanDisk;

/**
 *  disk 缓存占用的硬盘大小
 */
- (NSUInteger)getSize;

/**
 *  disk 缓存中的图片张数
 */
- (NSUInteger)getDiskCount;

/**
 *  异步计算 disk 缓存的大小
 */
- (void)calculateSizeWithCompletionBlock:(SDWebImageCalculateSizeBlock)completionBlock;

/**
 *  异步查看图片是否已经存在在 disk 缓存中，不会加载图片
 *
 *  @param key             描述 URL 的 key
 *  @param completionBlock 检查完成后就会在主线程调用的 block
 */
- (void)diskImageExistsWithKey:(NSString *)key completion:(SDWebImageCheckCacheCompletionBlock)completionBlock;

/**
 *  查看图片是否已经存在在 disk 缓存中，不会加载图片
 *
 *  @return 图片存在就返回 YES
 */
- (BOOL)diskImageExistsWithKey:(NSString *)key;

/**
 *  在给定的根文件夹下通过 key 查询图片缓存路径
 *
 *  @param path 缓存路径的根文件夹
 *
 *  @return 图片缓存路径
 */
- (NSString *)cachePathForKey:(NSString *)key inPath:(NSString *)path;

/**
 *  在默认的文件夹下查询图片缓存路径
 *
 *  @return 图片默认的缓存路径
 */
- (NSString *)defaultCachePathForKey:(NSString *)key;

@end
