#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <sys/param.h>
#include "ottd.h"

OSStatus GenerateThumbnailForURL(void *thisInterface, QLThumbnailRequestRef thumbnail, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CGSize maxSize);
void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail);
CGImageRef ottd_get_cgimage(const ottd_t *game, int mode);

/* -----------------------------------------------------------------------------
    Generate a thumbnail for file

   This function's job is to create thumbnail for designated file as fast as possible
   ----------------------------------------------------------------------------- */

CGImageRef CGimageResize(CGImageRef image, CGSize maxSize)
{
    // calcualte size
    CGFloat ratio = MAX(CGImageGetWidth(image)/maxSize.width,CGImageGetHeight(image)/maxSize.height);
    size_t width = CGImageGetWidth(image)/ratio;
    size_t height = CGImageGetHeight(image)/ratio;
    // resize
    CGColorSpaceRef colorspace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(NULL, width, height,
                                                 8, width*4, colorspace, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(colorspace);
    
    if(context == NULL) return nil;
    
    // draw image to context (resizing it)
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    // extract resulting image from context
    CGImageRef imgRef = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    
    return imgRef;
}

OSStatus GenerateThumbnailForURL(void *thisInterface, QLThumbnailRequestRef thumbnail, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options, CGSize maxSize)
{
    ottd_t *game = NULL;
    char path[MAXPATHLEN];
    
    // get path from URL
    if (!CFURLGetFileSystemRepresentation(url, true, (UInt8*)path, MAXPATHLEN)) goto fail;
    
    // load savegame
    game = ottd_load(path, 0);
    if (game == NULL) goto fail;
    
    // make map picture
    CGImageRef img = ottd_get_cgimage(game, OTTD_MAP_NW);
    
    // set thumbnail
    CGImageRef thumbnailImg = CGimageResize(img, maxSize);
    QLThumbnailRequestSetImage(thumbnail, thumbnailImg, NULL);
    
    // freedom
    CGImageRelease(img);
    CGImageRelease(thumbnailImg);
    ottd_free(game);
    return noErr;
fail:
    ottd_free(game);
    return noErr;
}

void CancelThumbnailGeneration(void *thisInterface, QLThumbnailRequestRef thumbnail)
{
    // Implement only if supported
}
