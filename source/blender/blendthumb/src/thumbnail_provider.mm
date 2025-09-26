/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#import <AppKit/NSImage.h>

#include "BLI_fileops.h"
#include "BLI_filereader.h"
#include "BLI_utility_mixins.hh"
#include "blendthumb.hh"

#include "thumbnail_provider.h"
/**
 * This section intends to list the important steps for creating a thumbnail extension.
 * qlgenerator has been deprecated and removed in platforms we support. App extensions are the way
 * forward. But there's little guidance on how to do it outside Xcode.
 *
 * The process of thumbnail generation goes something like this:
 * 1. If an app is launched, or is registered with lsregister, its plugins also get registered.
 * 2. When a file thumbnail in Finder or QuickLook is requested, the system looks for a plugin
 *    that supports the file type UTI.
 * 3. The plugin is launched in a sand-boxed environment and should call the handler with a reply.
 *
 * # Plugin Info.plist
 * The Info.plist file should be properly configured with supported content type.
 *
 * # Codesigning
 * The plugin should be codesigned with entitlements at least for sandbox and read-only/
 * read-write (for access to the given file). It's needed to even run the plugin locally.
 * com.apple.security.get-task-allow entitlement is required for debugging.
 *
 * # Registering the plugin
 * The plugin should be registered with lsregister. Either by calling lsregister or by launching
 * the parent app.
 * /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
 * \ -dump | grep blender-thumbnailer
 *
 * # Debugging
 * Since read-only entitlement is there, creating files to log is not possible. So NSLog and
 * viewing it in Console.app (after triggering a thumbnail) is the way to go. Interesting processes
 * are: qlmanage, quicklookd, kernel, blender-thumbnailer, secinitd,
 * com.apple.quicklook.ThumbnailsAgent
 *
 * LLDB/ Xcode etc., debuggers can be used to get extra logs than CLI invocation but breakpoints
 * still are a pain point. /usr/bin/qlmanage is the target executable. Other args to qlmanage
 * follow. lldb qlmanage --  -t -x a.blend
 *
 * # Troubleshooting
 * - The appex shouldn't have any quarantine flag.
 *   xattr -rl bin/Blender.app/Contents/Plugins/blender-thumbnailer.appex
 * - Is it registered with lsregister and there isn't a conflict with another plugin taking
 *   precedence? lsregister -dump | grep blender-thumbnailer.appex
 * - For RBSLaunchRequest error: is the executable flag set? chmod u+x
 * bin/Blender.app/Contents/PlugIns/blender-thumbnailer.appex/Contents/MacOS/blender-thumbnailer
 * - Is it codesigned and sandboxed?
 *   codesign --display --verbose --entitlements - --xml \
 * bin/Blender.app/Contents/Plugins/blender-thumbnailer.appex codesign --deep --force --sign - \
 * --entitlements ../blender/release/darwin/thumbnailer_entitlements.plist --timestamp=none \
 * bin/Blender.app/Contents/Plugins/blender-thumbnailer.appex
 * - Sometimes blender-thumbnailer running in background can be killed.
 * - qlmanage -r && killall Finder
 * - The code cannot attempt to do anything outside sandbox like writing to blend.
 *
 * # Triggering a thumbnail
 * - qlmanage -t -x /path/to/file.blend
 *
 * # External resources
 * https://developer.apple.com/library/archive/documentation/UserExperience/Conceptual/Quicklook_Programming_Guide/Introduction/Introduction.html#//apple_ref/doc/uid/TP40005020-CH1-SW1
 */

class FileDescriptorRAII : blender::NonCopyable, blender::NonMovable {
 private:
  int src_fd = -1;

 public:
  explicit FileDescriptorRAII(const char *file_path)
  {
    src_fd = BLI_open(file_path, O_BINARY | O_RDONLY, 0);
  }

  ~FileDescriptorRAII()
  {
    if (good()) {
      int ok = close(src_fd);
      if (!ok) {
        NSLog(@"Blender Thumbnailer Error: Failed to close the blend file.");
      }
    }
  }

  bool good()
  {
    return src_fd > 0;
  }

  int get()
  {
    return src_fd;
  }
};

static NSError *create_nserror_from_string(NSString *errorStr)
{
  NSLog(@"Blender Thumbnailer Error: %@", errorStr);
  return [NSError errorWithDomain:@"org.blenderfoundation.blender.thumbnailer"
                             code:-1
                         userInfo:@{NSLocalizedDescriptionKey : errorStr}];
}

static NSImage *generate_nsimage_for_file(const char *src_blend_path, NSError *error)
{
  /* Open source file `src_blend`. */
  FileDescriptorRAII src_file_fd = FileDescriptorRAII(src_blend_path);
  if (!src_file_fd.good()) {
    error = create_nserror_from_string(@"Failed to open blend");
    return nil;
  }

  FileReader *file_content = BLI_filereader_new_file(src_file_fd.get());
  if (file_content == nullptr) {
    error = create_nserror_from_string(@"Failed to read from blend");
    return nil;
  }

  /* Extract thumbnail from file. */
  Thumbnail thumb;
  eThumbStatus err = blendthumb_create_thumb_from_file(file_content, &thumb);
  if (err != BT_OK) {
    error = create_nserror_from_string(@"Failed to create thumbnail from file");
    return nil;
  }

  std::optional<blender::Vector<uint8_t>> png_buf_opt = blendthumb_create_png_data_from_thumb(
      &thumb);
  if (!png_buf_opt) {
    error = create_nserror_from_string(@"Failed to create png data from thumbnail");
    return nil;
  }

  NSData *ns_data = [NSData dataWithBytes:png_buf_opt->data() length:png_buf_opt->size()];
  NSImage *ns_image = [[NSImage alloc] initWithData:ns_data];
  return ns_image;
}

@implementation ThumbnailProvider

- (void)provideThumbnailForFileRequest:(QLFileThumbnailRequest *)request
                     completionHandler:(void (^)(QLThumbnailReply *_Nullable reply,
                                                 NSError *_Nullable error))handler
{

  NSLog(@"Generating thumbnail for %@", request.fileURL.path);
  @autoreleasepool {
    NSError *error = nil;
    NSImage *image = generate_nsimage_for_file(request.fileURL.path.fileSystemRepresentation,
                                               error);
    if (image == nil || image.size.width <= 0 || image.size.height <= 0) {
      handler(nil, error);
      return;
    }

    const CGFloat width_ratio = request.maximumSize.width / image.size.width;
    const CGFloat height_ratio = request.maximumSize.height / image.size.height;
    const CGFloat scale_factor = MIN(width_ratio, height_ratio);

    const NSSize context_size = NSMakeSize(image.size.width * scale_factor,
                                           image.size.height * scale_factor);

    const NSRect context_rect = NSMakeRect(0, 0, context_size.width, context_size.height);

    QLThumbnailReply *thumbnailReply = [QLThumbnailReply replyWithContextSize:context_size
                                                   currentContextDrawingBlock:^BOOL {
                                                     [image drawInRect:context_rect];
                                                     /* Release the image that was strongly
                                                      * captured by this block. */
                                                     [image release];
                                                     return YES;
                                                   }];

    /* Return the thumbnail reply. */
    handler(thumbnailReply, nil);
  }
  NSLog(@"Thumbnail generation successfully completed");
}

@end
