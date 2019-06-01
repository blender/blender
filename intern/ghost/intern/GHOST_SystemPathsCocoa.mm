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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 *
 */

#import <Foundation/Foundation.h>

#include "GHOST_SystemPathsCocoa.h"

#pragma mark initialization/finalization

GHOST_SystemPathsCocoa::GHOST_SystemPathsCocoa()
{
}

GHOST_SystemPathsCocoa::~GHOST_SystemPathsCocoa()
{
}

#pragma mark Base directories retrieval

const GHOST_TUns8 *GHOST_SystemPathsCocoa::getSystemDir(int, const char *versionstr) const
{
  static char tempPath[512] = "";
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSString *basePath;
  NSArray *paths;

  paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSLocalDomainMask, YES);

  if ([paths count] > 0)
    basePath = [paths objectAtIndex:0];
  else {
    [pool drain];
    return NULL;
  }

  snprintf(tempPath,
           sizeof(tempPath),
           "%s/Blender/%s",
           [basePath cStringUsingEncoding:NSASCIIStringEncoding],
           versionstr);

  [pool drain];
  return (GHOST_TUns8 *)tempPath;
}

const GHOST_TUns8 *GHOST_SystemPathsCocoa::getUserDir(int, const char *versionstr) const
{
  static char tempPath[512] = "";
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSString *basePath;
  NSArray *paths;

  paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, YES);

  if ([paths count] > 0)
    basePath = [paths objectAtIndex:0];
  else {
    [pool drain];
    return NULL;
  }

  snprintf(tempPath,
           sizeof(tempPath),
           "%s/Blender/%s",
           [basePath cStringUsingEncoding:NSASCIIStringEncoding],
           versionstr);

  [pool drain];
  return (GHOST_TUns8 *)tempPath;
}

const GHOST_TUns8 *GHOST_SystemPathsCocoa::getBinaryDir() const
{
  static GHOST_TUns8 tempPath[512] = "";
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSString *basePath;

  basePath = [[NSBundle mainBundle] bundlePath];

  if (basePath == nil) {
    [pool drain];
    return NULL;
  }

  strcpy((char *)tempPath, [basePath cStringUsingEncoding:NSASCIIStringEncoding]);

  [pool drain];
  return tempPath;
}

void GHOST_SystemPathsCocoa::addToSystemRecentFiles(const char *filename) const
{
  /* TODO: implement for macOS */
}
