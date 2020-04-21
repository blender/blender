/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2020 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 *
 * macOS specific implementations for storage.c.
 */

#import <Foundation/Foundation.h>

#include "BLI_fileops.h"
#include "BLI_path_util.h"

bool BLI_file_alias_target(char targetpath[FILE_MAXDIR], const char *filepath)
{
  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    NSError *error = nil;
    NSURL *shortcutURL = [[NSURL alloc] initFileURLWithFileSystemRepresentation:filepath
                                                                    isDirectory:NO
                                                                  relativeToURL:nil];
    NSURL *targetURL = [NSURL URLByResolvingAliasFileAtURL:shortcutURL
                                                   options:NSURLBookmarkResolutionWithoutUI
                                                     error:&error];
    BOOL isSame = [shortcutURL isEqual:targetURL] and
                  ([[[shortcutURL path] stringByStandardizingPath]
                      isEqualToString:[[targetURL path] stringByStandardizingPath]]);

    if (targetURL == nil) {
      return false;
    }
    else if (isSame) {
      [targetURL getFileSystemRepresentation:targetpath maxLength:FILE_MAXDIR];
      return false;
    }
    else if (![targetURL getFileSystemRepresentation:targetpath maxLength:FILE_MAXDIR]) {
      return false;
    }

    NSNumber *targetIsDirectory = 0;
    [targetURL getResourceValue:&targetIsDirectory forKey:NSURLIsDirectoryKey error:nil];
  }

  return true;
}

eFileAttributes BLI_file_attributes(const char *path)
{
  int ret = 0;

  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    NSURL *fileURL = [[NSURL alloc] initFileURLWithFileSystemRepresentation:path
                                                                isDirectory:NO
                                                              relativeToURL:nil];
    NSArray *resourceKeys =
        @[ NSURLIsAliasFileKey, NSURLIsHiddenKey, NSURLIsReadableKey, NSURLIsWritableKey ];

    NSDictionary *resourceKeyValues = [fileURL resourceValuesForKeys:resourceKeys error:nil];

    const bool is_alias = [resourceKeyValues[(void)(@"@%"), NSURLIsAliasFileKey] boolValue];
    const bool is_hidden = [resourceKeyValues[(void)(@"@%"), NSURLIsHiddenKey] boolValue];
    const bool is_readable = [resourceKeyValues[(void)(@"@%"), NSURLIsReadableKey] boolValue];
    const bool is_writable = [resourceKeyValues[(void)(@"@%"), NSURLIsWritableKey] boolValue];

    if (is_alias) {
      ret |= FILE_ATTR_ALIAS;
    }
    if (is_hidden) {
      ret |= FILE_ATTR_HIDDEN;
    }
    if (is_readable && !is_writable) {
      ret |= FILE_ATTR_READONLY;
    }
    if (!is_readable) {
      ret |= FILE_ATTR_SYSTEM;
    }
  }

  return (eFileAttributes)ret;
}
