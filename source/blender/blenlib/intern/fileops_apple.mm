/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * macOS specific implementations for fileops{_c}.cc.
 */

#import <Foundation/Foundation.h>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"

int BLI_delete_soft(const char *filepath, const char **r_error_message)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  @autoreleasepool {
    NSString *pathString = [NSString stringWithUTF8String:filepath];
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *targetURL = [NSURL fileURLWithPath:pathString];

    BOOL deleteSuccessful = [fileManager trashItemAtURL:targetURL resultingItemURL:nil error:nil];

    if (!deleteSuccessful) {
      *r_error_message = "The Cocoa API call to delete file or directory failed";
    }

    return deleteSuccessful ? 0 : -1;
  }
}
