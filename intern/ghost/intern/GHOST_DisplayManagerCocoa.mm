/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2009 Damien Plisson
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <Cocoa/Cocoa.h>

#include "GHOST_Debug.hh"
#include "GHOST_DisplayManagerCocoa.hh"

// We do not support multiple monitors at the moment

GHOST_DisplayManagerCocoa::GHOST_DisplayManagerCocoa(void) {}

GHOST_TSuccess GHOST_DisplayManagerCocoa::getNumDisplays(uint8_t &numDisplays) const
{
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  numDisplays = (uint8_t)[[NSScreen screens] count];

  [pool drain];
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerCocoa::getNumDisplaySettings(uint8_t /*display*/,
                                                                int32_t &numSettings) const
{
  numSettings = (int32_t)3;  // Width, Height, BitsPerPixel

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_DisplayManagerCocoa::getDisplaySetting(uint8_t display,
                                                            int32_t /*index*/,
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
    uint8_t display, GHOST_DisplaySetting &setting) const
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
    uint8_t display, const GHOST_DisplaySetting & /*setting*/)
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

  /* Display configuration is no more available in 10.6. */

#if 0
  CFDictionaryRef displayModeValues = ::CGDisplayBestModeForParametersAndRefreshRate(
      m_displayIDs[display],
      (size_t)setting.bpp,
      (size_t)setting.xPixels,
      (size_t)setting.yPixels,
      (CGRefreshRate)setting.frequency,
      NULL);
#endif

#ifdef GHOST_DEBUG
#  if 0
  printf("GHOST_DisplayManagerCocoa::setCurrentDisplaySetting(): switching to:\n");
  printf("  setting.xPixels=%d\n", getValue(displayModeValues, kCGDisplayWidth));
  printf("  setting.yPixels=%d\n", getValue(displayModeValues, kCGDisplayHeight));
  printf("  setting.bpp=%d\n", getValue(displayModeValues, kCGDisplayBitsPerPixel));
  printf("  setting.frequency=%d\n", getValue(displayModeValues, kCGDisplayRefreshRate));
#  endif
#endif  // GHOST_DEBUG

  // CGDisplayErr err = ::CGDisplaySwitchToMode(m_displayIDs[display], displayModeValues);

  return /* err == CGDisplayNoErr ? */ GHOST_kSuccess /* : GHOST_kFailure */;
}
