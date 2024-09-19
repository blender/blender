/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#import <AppKit/NSDocumentController.h>
#import <Foundation/Foundation.h>

#include "GHOST_Debug.hh"
#include "GHOST_SystemPathsCocoa.hh"

/* --------------------------------------------------------------------
 * Base directories retrieval.
 */

static const char *GetApplicationSupportDir(const char *versionstr,
                                            const NSSearchPathDomainMask mask,
                                            char *tempPath,
                                            const std::size_t len_tempPath)
{
  @autoreleasepool {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, mask, YES);

    if (paths.count == 0) {
      return nullptr;
    }
    NSString *basePath = [paths objectAtIndex:0];

    snprintf(tempPath,
             len_tempPath,
             "%s/Blender/%s",
             [basePath cStringUsingEncoding:NSASCIIStringEncoding],
             versionstr);
  }
  return tempPath;
}

const char *GHOST_SystemPathsCocoa::getSystemDir(int /* version */, const char *versionstr) const
{
  static char tempPath[512] = "";
  return GetApplicationSupportDir(versionstr, NSLocalDomainMask, tempPath, sizeof(tempPath));
}

const char *GHOST_SystemPathsCocoa::getUserDir(int /* version */, const char *versionstr) const
{
  static char tempPath[512] = "";
  return GetApplicationSupportDir(versionstr, NSUserDomainMask, tempPath, sizeof(tempPath));
}

const char *GHOST_SystemPathsCocoa::getUserSpecialDir(GHOST_TUserSpecialDirTypes type) const
{
  static char tempPath[512] = "";
  @autoreleasepool {
    NSSearchPathDirectory ns_directory;

    switch (type) {
      case GHOST_kUserSpecialDirDesktop:
        ns_directory = NSDesktopDirectory;
        break;
      case GHOST_kUserSpecialDirDocuments:
        ns_directory = NSDocumentDirectory;
        break;
      case GHOST_kUserSpecialDirDownloads:
        ns_directory = NSDownloadsDirectory;
        break;
      case GHOST_kUserSpecialDirMusic:
        ns_directory = NSMusicDirectory;
        break;
      case GHOST_kUserSpecialDirPictures:
        ns_directory = NSPicturesDirectory;
        break;
      case GHOST_kUserSpecialDirVideos:
        ns_directory = NSMoviesDirectory;
        break;
      case GHOST_kUserSpecialDirCaches:
        ns_directory = NSCachesDirectory;
        break;
      default:
        GHOST_ASSERT(
            false,
            "GHOST_SystemPathsCocoa::getUserSpecialDir(): Invalid enum value for type parameter");
        return nullptr;
    }

    NSArray *paths = NSSearchPathForDirectoriesInDomains(ns_directory, NSUserDomainMask, YES);
    if (paths.count == 0) {
      return nullptr;
    }
    NSString *basePath = [paths objectAtIndex:0];

    const char *basePath_cstr = [basePath cStringUsingEncoding:NSASCIIStringEncoding];
    int basePath_len = strlen(basePath_cstr);

    basePath_len = MIN(basePath_len, sizeof(tempPath) - 1);
    memcpy(tempPath, basePath_cstr, basePath_len);
    tempPath[basePath_len] = '\0';
  }
  return tempPath;
}

const char *GHOST_SystemPathsCocoa::getBinaryDir() const
{
  static char tempPath[512] = "";

  @autoreleasepool {
    NSString *basePath = [[NSBundle mainBundle] bundlePath];

    if (basePath == nil) {
      return nullptr;
    }

    const char *basePath_cstr = [basePath cStringUsingEncoding:NSASCIIStringEncoding];
    int basePath_len = strlen(basePath_cstr);

    basePath_len = MIN(basePath_len, sizeof(tempPath) - 1);
    memcpy(tempPath, basePath_cstr, basePath_len);
    tempPath[basePath_len] = '\0';
  }
  return tempPath;
}

void GHOST_SystemPathsCocoa::addToSystemRecentFiles(const char *filepath) const
{
  @autoreleasepool {
    NSURL *file_url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filepath]];
    [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:file_url];
  }
}
