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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
          Damien Plisson  10/2009
 */

#include <Cocoa/Cocoa.h>

#include "GHOST_DisplayManagerCocoa.h"
#include "GHOST_Debug.h"

// We do not support multiple monitors at the moment

GHOST_DisplayManagerCocoa::GHOST_DisplayManagerCocoa(void)
{
}

GHOST_TSuccess GHOST_DisplayManagerCocoa::getNumDisplays(GHOST_TUns8 &numDisplays) const
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  numDisplays = (GHOST_TUns8)[[NSScreen screens] count];

  [pool drain];
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerCocoa::getNumDisplaySettings(GHOST_TUns8 display,
                                                                GHOST_TInt32 &numSettings) const
{
  numSettings = (GHOST_TInt32)3;  // Width, Height, BitsPerPixel

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerCocoa::getDisplaySetting(GHOST_TUns8 display,
                                                            GHOST_TInt32 index,
                                                            GHOST_DisplaySetting &setting) const
{
  NSScreen *askedDisplay;

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  if (display == kMainDisplay)  // Screen #0 may not be the main one
    askedDisplay = [NSScreen mainScreen];
  else
    askedDisplay = [[NSScreen screens] objectAtIndex:display];

  if (askedDisplay == nil) {
    [pool drain];
    return GHOST_kFailure;
  }

  NSRect frame = [askedDisplay visibleFrame];
  setting.xPixels = frame.size.width;
  setting.yPixels = frame.size.height;

  setting.bpp = NSBitsPerPixelFromDepth([askedDisplay depth]);

  setting.frequency = 0;  // No more CRT display...

#ifdef GHOST_DEBUG
  printf("display mode: width=%d, height=%d, bpp=%d, frequency=%d\n",
         setting.xPixels,
         setting.yPixels,
         setting.bpp,
         setting.frequency);
#endif  // GHOST_DEBUG

  [pool drain];
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerCocoa::getCurrentDisplaySetting(
    GHOST_TUns8 display, GHOST_DisplaySetting &setting) const
{
  NSScreen *askedDisplay;

  GHOST_ASSERT(
      (display == kMainDisplay),
      "GHOST_DisplayManagerCocoa::getCurrentDisplaySetting(): only main display is supported");

  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  if (display == kMainDisplay)  // Screen #0 may not be the main one
    askedDisplay = [NSScreen mainScreen];
  else
    askedDisplay = [[NSScreen screens] objectAtIndex:display];

  if (askedDisplay == nil) {
    [pool drain];
    return GHOST_kFailure;
  }

  NSRect frame = [askedDisplay visibleFrame];
  setting.xPixels = frame.size.width;
  setting.yPixels = frame.size.height;

  setting.bpp = NSBitsPerPixelFromDepth([askedDisplay depth]);

  setting.frequency = 0;  // No more CRT display...

#ifdef GHOST_DEBUG
  printf("current display mode: width=%d, height=%d, bpp=%d, frequency=%d\n",
         setting.xPixels,
         setting.yPixels,
         setting.bpp,
         setting.frequency);
#endif  // GHOST_DEBUG

  [pool drain];
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(
    GHOST_TUns8 display, const GHOST_DisplaySetting &setting)
{
  GHOST_ASSERT(
      (display == kMainDisplay),
      "GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(): only main display is supported");

#ifdef GHOST_DEBUG
  printf("GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(): requested settings:\n");
  printf("  setting.xPixels=%d\n", setting.xPixels);
  printf("  setting.yPixels=%d\n", setting.yPixels);
  printf("  setting.bpp=%d\n", setting.bpp);
  printf("  setting.frequency=%d\n", setting.frequency);
#endif  // GHOST_DEBUG

  // Display configuration is no more available in 10.6

  /*  CFDictionaryRef displayModeValues = ::CGDisplayBestModeForParametersAndRefreshRate(
    m_displayIDs[display],
    (size_t)setting.bpp,
    (size_t)setting.xPixels,
    (size_t)setting.yPixels,
    (CGRefreshRate)setting.frequency,
    NULL);*/

#ifdef GHOST_DEBUG
/*  printf("GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(): switching to:\n");
  printf("  setting.xPixels=%d\n", getValue(displayModeValues, kCGDisplayWidth));
  printf("  setting.yPixels=%d\n", getValue(displayModeValues, kCGDisplayHeight));
  printf("  setting.bpp=%d\n", getValue(displayModeValues, kCGDisplayBitsPerPixel));
  printf("  setting.frequency=%d\n", getValue(displayModeValues, kCGDisplayRefreshRate)); */
#endif  // GHOST_DEBUG

  // CGDisplayErr err = ::CGDisplaySwitchToMode(m_displayIDs[display], displayModeValues);

  return /*err == CGDisplayNoErr ?*/ GHOST_kSuccess /*: GHOST_kFailure*/;
}
