#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <QuickLook/QuickLook.h>
#include <sys/param.h>
#include "ottd.h"

OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options);
void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview);
CGImageRef ottd_get_cgimage(const ottd_t *game, int mode);

#define kTextLeft 16.0f
#define kTextFont "Helvetica-Bold"
#define kTextSize 14.0f
#define kTextLineHeight 18.0f

void DrawCompanyName(CGContextRef ctx, const char *name, int color, CGFloat y)
{
    // fuck yeah core text
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGMutablePathRef path = CGPathCreateMutable();
    CGRect bounds = CGRectMake(kTextLeft, y+(kTextLineHeight/2), 300.0f, kTextLineHeight);
    CGPathAddRect(path, NULL, bounds);

    // Initialize an attributed string.
    CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, name, kCFStringEncodingUTF8);
    CFMutableAttributedStringRef attrString = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
    CFAttributedStringReplaceString (attrString, CFRangeMake(0, 0), string);
    CFRelease(string);

    // Create a color and add it as an attribute to the string.
    CGColorSpaceRef rgbColorSpace = CGColorSpaceCreateDeviceRGB();
    png_color cc = ottd_color[ottd_company_color(color)];
    CGFloat components[] = { cc.red/255.0f, cc.green/255.0f, cc.blue/255.0f, 1.0f };
    CGColorRef tc = CGColorCreate(rgbColorSpace, components);
    CGColorSpaceRelease(rgbColorSpace);
    CFRange stringRange = CFRangeMake(0, CFAttributedStringGetLength(attrString));
    CFAttributedStringSetAttribute(attrString, stringRange, kCTForegroundColorAttributeName, tc);
    CGColorRelease(tc);

    // font
    CFTypeRef attr = CTFontCreateWithName(CFSTR(kTextFont), kTextSize, NULL);
    CFAttributedStringSetAttribute(attrString, stringRange, kCTFontAttributeName, attr);
    CFRelease(attr);

    // Create the framesetter with the attributed string.
    CTFramesetterRef framesetter = CTFramesetterCreateWithAttributedString(attrString);
    CFRelease(attrString);

    // Create the frame and draw it into the graphics context
    CTFrameRef frame = CTFramesetterCreateFrame(framesetter, CFRangeMake(0, 0), path, NULL);
    CFRelease(framesetter);
    CTFrameDraw(frame, ctx);
    CFRelease(frame);
}

OSStatus GeneratePreviewForURL(void *thisInterface, QLPreviewRequestRef preview, CFURLRef url, CFStringRef contentTypeUTI, CFDictionaryRef options)
{
    ottd_t *game = NULL;
    char path[MAXPATHLEN];
    
    // get path from URL
    if (!CFURLGetFileSystemRepresentation(url, true, (UInt8*)path, MAXPATHLEN)) goto fail;
    
    // load savegame
    game = ottd_load(path, 0);
    if (game == NULL) goto fail;
    
    // get map image
    CGImageRef img = ottd_get_cgimage(game, OTTD_MAP_ISO);
    CGRect imgRect = CGRectMake(0, 0, CGImageGetWidth(img), CGImageGetHeight(img));
    CGRect tableRect = CGRectMake(kTextLeft, 0, 240, 200);
    CGSize size = imgRect.size;
    if (imgRect.size.height < 512) {
        size.height = 512;
        imgRect.origin.y = (size.height - imgRect.size.height)/2;
    }
    CGFloat minWidth = tableRect.size.width*2+tableRect.origin.x*3;
    if (imgRect.size.width < minWidth) {
        size.width = minWidth;
        imgRect.origin.x = (size.width - imgRect.size.width)/2;
    }
    tableRect.origin.y = size.height - tableRect.size.height - 16;
    
    // draw preview
    CGContextRef ctx = QLPreviewRequestCreateContext(preview, size, true, NULL);
    CGContextSetFillColorWithColor(ctx, CGColorGetConstantColor(kCGColorBlack));
    CGContextFillRect(ctx, CGRectMake(0, 0, size.width, size.height));
    CGContextDrawImage(ctx, imgRect, img);
    
    // draw text
    char yearsText[32];
    snprintf(yearsText, sizeof yearsText, "%d-%d", game->startYear, game->curDate.year);
    DrawCompanyName(ctx, yearsText, 14, tableRect.origin.y + tableRect.size.height - kTextLineHeight*1);
    int companyLine = 2;
    for(int i=0; i < 15; i++) {
        ottd_company_t *cmp= &game->company[i];
        if (!cmp->active) continue;
        DrawCompanyName(ctx, cmp->name, cmp->color, tableRect.origin.y + tableRect.size.height - kTextLineHeight*companyLine++);
    }
    
    // finish drawing
    QLPreviewRequestFlushContext(preview, ctx);
    CGContextRelease(ctx);
    
    ottd_free(game);
    CGImageRelease(img);
    return noErr;
fail:
    ottd_free(game);
    return noErr;
}

void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview)
{
    // Implement only if supported
}
