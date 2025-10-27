/* SPDX-FileCopyrightText: 2020 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 *
 * macOS specific implementations for storage.c.
 */

#import <Foundation/Foundation.h>
#include <string>
#include <sys/xattr.h>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

/* Extended file attribute used by OneDrive to mark placeholder files. */
static const char *ONEDRIVE_RECALLONOPEN_ATTRIBUTE = "com.microsoft.OneDrive.RecallOnOpen";

bool BLI_file_alias_target(const char *filepath,
                           /* False alarm by clang-tidy: #getFileSystemRepresentation
                            * changes the return value argument. */
                           /* NOLINTNEXTLINE: readability-non-const-parameter. */
                           char r_targetpath[FILE_MAXDIR])
{
  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    NSError *error = nil;
    NSURL *shortcutURL = [[NSURL alloc] initFileURLWithFileSystemRepresentation:filepath
                                                                    isDirectory:NO
                                                                  relativeToURL:nil];

    /* Note, NSURLBookmarkResolutionWithoutMounting keeps blender from crashing when an alias can't
     * be mounted */
    NSURL *targetURL = [NSURL URLByResolvingAliasFileAtURL:shortcutURL
                                                   options:NSURLBookmarkResolutionWithoutUI |
                                                           NSURLBookmarkResolutionWithoutMounting
                                                     error:&error];
    const BOOL isSame = [shortcutURL isEqual:targetURL] and
                        ([[[shortcutURL path] stringByStandardizingPath]
                            isEqualToString:[[targetURL path] stringByStandardizingPath]]);

    if (targetURL == nil) {
      return false;
    }
    if (isSame) {
      [targetURL getFileSystemRepresentation:r_targetpath maxLength:FILE_MAXDIR];
      return false;
    }
    /* Note that the `if` condition may also change the value of `r_targetpath`. */
    if (![targetURL getFileSystemRepresentation:r_targetpath maxLength:FILE_MAXDIR]) {
      return false;
    }
  }

  return true;
}

/**
 * Checks if the given string of listxattr() attributes contains a specific attribute.
 *
 * \param attributes: a string of null-terminated listxattr() attributes.
 * \param search_attribute: the attribute to search for.
 * \return 'true' when the attribute is found, otherwise 'false'.
 */
static bool find_attribute(const std::string &attributes, const char *search_attribute)
{
  /* Attributes is a list of consecutive null-terminated strings. */
  const char *end = attributes.data() + attributes.size();
  for (const char *item = attributes.data(); item < end; item += strlen(item) + 1) {
    if (STREQ(item, search_attribute)) {
      return true;
    }
  }

  return false;
}

/**
 * Checks if the file is merely a placeholder for a OneDrive file that hasn't yet been downloaded.
 *
 * \param path: the path of the file.
 * \return 'true' when the file is a OneDrive placeholder, otherwise 'false'.
 */
static bool test_onedrive_file_is_placeholder(const char *path)
{
  /* NOTE: Currently only checking for the "com.microsoft.OneDrive.RecallOnOpen" extended file
   * attribute. In theory this attribute can also be set on files that aren't located inside a
   * OneDrive folder. Maybe additional checks are required? */

  /* Get extended file attributes */
  ssize_t size = listxattr(path, nullptr, 0, XATTR_NOFOLLOW);
  if (size < 1) {
    return false;
  }

  std::string attributes(size, '\0');
  size = listxattr(path, attributes.data(), size, XATTR_NOFOLLOW);
  /* In case listxattr() has failed the second time it's called. */
  if (size < 1) {
    return false;
  }

  /* Check for presence of 'com.microsoft.OneDrive.RecallOnOpen' attribute. */
  return find_attribute(attributes, ONEDRIVE_RECALLONOPEN_ATTRIBUTE);
}

/**
 * Checks if the file is marked as offline and not immediately available.
 *
 * \param path: the path of the file.
 * \return 'true' when the file is a placeholder, otherwise 'false'.
 */
static bool test_file_is_offline(const char *path)
{
  /* Logic for additional cloud storage providers could be added in the future. */
  return test_onedrive_file_is_placeholder(path);
}

eFileAttributes BLI_file_attributes(const char *path)
{
  int ret = 0;

  /* clang-format off */
  @autoreleasepool {
    /* clang-format on */
    NSURL *fileURL = [[[NSURL alloc] initFileURLWithFileSystemRepresentation:path
                                                                 isDirectory:NO
                                                               relativeToURL:nil] autorelease];

    /* Querying NSURLIsReadableKey and NSURLIsWritableKey keys for OneDrive placeholder files
     * triggers their unwanted download. */
    NSArray *resourceKeys = nullptr;
    const bool is_offline = test_file_is_offline(path);

    if (is_offline) {
      resourceKeys = @[ NSURLIsSymbolicLinkKey, NSURLIsAliasFileKey, NSURLIsHiddenKey ];
    }
    else {
      resourceKeys = @[
        NSURLIsSymbolicLinkKey,
        NSURLIsAliasFileKey,
        NSURLIsHiddenKey,
        NSURLIsReadableKey,
        NSURLIsWritableKey
      ];
    }

    NSDictionary *resourceKeyValues = [fileURL resourceValuesForKeys:resourceKeys error:nil];

    const bool is_symlink = [resourceKeyValues[(void)(@"@%"), NSURLIsSymbolicLinkKey] boolValue];
    const bool is_alias = [resourceKeyValues[(void)(@"@%"), NSURLIsAliasFileKey] boolValue] &&
                          !is_symlink;
    const bool is_hidden = [resourceKeyValues[(void)(@"@%"), NSURLIsHiddenKey] boolValue];
    const bool is_readable = is_offline ||
                             [resourceKeyValues[(void)(@"@%"), NSURLIsReadableKey] boolValue];
    const bool is_writable = is_offline ||
                             [resourceKeyValues[(void)(@"@%"), NSURLIsWritableKey] boolValue];

    if (is_symlink) {
      ret |= FILE_ATTR_SYMLINK;
    }
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
    if (is_offline) {
      ret |= FILE_ATTR_OFFLINE;
    }
  }

  return (eFileAttributes)ret;
}

char *BLI_current_working_dir(char *dir, const size_t maxncpy)
{
  /* Can't just copy to the *dir pointer, as [path getCString gets grumpy. */
  char path_expanded[PATH_MAX];
  @autoreleasepool {
    NSString *path = [[NSFileManager defaultManager] currentDirectoryPath];
    const size_t length = maxncpy > PATH_MAX ? PATH_MAX : maxncpy;
    [path getCString:path_expanded maxLength:length encoding:NSUTF8StringEncoding];
    BLI_strncpy(dir, path_expanded, maxncpy);
    return dir;
  }
}

bool BLI_change_working_dir(const char *dir)
{
  @autoreleasepool {
    NSString *path = [[NSString alloc] initWithUTF8String:dir];
    if ([[NSFileManager defaultManager] changeCurrentDirectoryPath:path] == YES) {
      return true;
    }
    return false;
  }
}
