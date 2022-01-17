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
 */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_WintabWin32 class.
 */

/* Wacom's Wintab documentation is periodically offline, moved, and increasingly hidden away. You
 * can find a (painstakingly) archived copy of the documentation at
 * https://web.archive.org/web/20201122230125/https://developer-docs-legacy.wacom.com/display/DevDocs/Windows+Wintab+Documentation
 */

#pragma once

#include <memory>
#include <vector>
#include <wtypes.h>

#include "GHOST_Types.h"

#include <wintab.h>
/* PACKETDATA and PACKETMODE modify structs in pktdef.h, so make sure they come first. */
#define PACKETDATA \
  (PK_BUTTONS | PK_NORMAL_PRESSURE | PK_ORIENTATION | PK_CURSOR | PK_X | PK_Y | PK_TIME)
#define PACKETMODE 0
#include <pktdef.h>

/* Typedefs for Wintab functions to allow dynamic loading. */
typedef UINT(API *GHOST_WIN32_WTInfo)(UINT, UINT, LPVOID);
typedef BOOL(API *GHOST_WIN32_WTGet)(HCTX, LPLOGCONTEXTA);
typedef BOOL(API *GHOST_WIN32_WTSet)(HCTX, LPLOGCONTEXTA);
typedef HCTX(API *GHOST_WIN32_WTOpen)(HWND, LPLOGCONTEXTA, BOOL);
typedef BOOL(API *GHOST_WIN32_WTClose)(HCTX);
typedef int(API *GHOST_WIN32_WTPacketsGet)(HCTX, int, LPVOID);
typedef int(API *GHOST_WIN32_WTQueueSizeGet)(HCTX);
typedef BOOL(API *GHOST_WIN32_WTQueueSizeSet)(HCTX, int);
typedef BOOL(API *GHOST_WIN32_WTEnable)(HCTX, BOOL);
typedef BOOL(API *GHOST_WIN32_WTOverlap)(HCTX, BOOL);

/* Typedefs for Wintab and Windows resource management. */
typedef std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&::FreeLibrary)> unique_hmodule;
typedef std::unique_ptr<std::remove_pointer_t<HCTX>, GHOST_WIN32_WTClose> unique_hctx;

struct GHOST_WintabInfoWin32 {
  int32_t x = 0;
  int32_t y = 0;
  GHOST_TEventType type = GHOST_kEventCursorMove;
  GHOST_TButtonMask button = GHOST_kButtonMaskNone;
  uint64_t time = 0;
  GHOST_TabletData tabletData = GHOST_TABLET_DATA_NONE;
};

class GHOST_Wintab {
 public:
  /**
   * Loads Wintab if available.
   * \param hwnd: Window to attach Wintab context to.
   */
  static GHOST_Wintab *loadWintab(HWND hwnd);

  /**
   * Enables Wintab context.
   */
  void enable();

  /**
   * Disables the Wintab context and unwinds Wintab state.
   */
  void disable();

  /**
   * Brings Wintab context to the top of the overlap order.
   */
  void gainFocus();

  /**
   * Puts Wintab context at bottom of overlap order and unwinds Wintab state.
   */
  void loseFocus();

  /**
   * Clean up when Wintab leaves tracking range.
   */
  void leaveRange();

  /**
   * Handle Wintab coordinate changes when DisplayChange events occur.
   */
  void remapCoordinates();

  /**
   * Maps Wintab to Win32 display coordinates.
   * \param x_in: The tablet x coordinate.
   * \param y_in: The tablet y coordinate.
   * \param x_out: Output for the Win32 mapped x coordinate.
   * \param y_out: Output for the Win32 mapped y coordinate.
   */
  void mapWintabToSysCoordinates(int x_in, int y_in, int &x_out, int &y_out);

  /**
   * Updates cached Wintab properties for current cursor.
   */
  void updateCursorInfo();

  /**
   * Handle Wintab info changes such as change in number of connected tablets.
   * \param lParam: LPARAM of the event.
   */
  void processInfoChange(LPARAM lParam);

  /**
   * Whether Wintab devices are present.
   * \return True if Wintab devices are present.
   */
  bool devicesPresent();

  /**
   * Translate Wintab packets into GHOST_WintabInfoWin32 structs.
   * \param outWintabInfo: Storage to return resulting GHOST_WintabInfoWin32 data.
   */
  void getInput(std::vector<GHOST_WintabInfoWin32> &outWintabInfo);

  /**
   * Whether Wintab coordinates should be trusted.
   * \return True if Wintab coordinates should be trusted.
   */
  bool trustCoordinates();

  /**
   * Tests whether Wintab coordinates can be trusted by comparing Win32 and Wintab reported cursor
   * position.
   * \param sysX: System cursor x position.
   * \param sysY: System cursor y position.
   * \param wtX: Wintab cursor x position.
   * \param wtY: Wintab cursor y position.
   * \return True if Win32 and Wintab cursor positions match within tolerance.
   *
   * NOTE: Only test coordinates on button press, not release. This prevents issues when async
   * mismatch causes mouse movement to replay and snap back, which is only an issue while drawing.
   */
  bool testCoordinates(int sysX, int sysY, int wtX, int wtY);

  /**
   * Retrieve the most recent tablet data, or none if pen is not in range.
   * \return Most recent tablet data, or none if pen is not in range.
   */
  GHOST_TabletData getLastTabletData();

 private:
  /** Wintab DLL handle. */
  unique_hmodule m_handle;
  /** Wintab API functions. */
  GHOST_WIN32_WTInfo m_fpInfo = nullptr;
  GHOST_WIN32_WTGet m_fpGet = nullptr;
  GHOST_WIN32_WTSet m_fpSet = nullptr;
  GHOST_WIN32_WTPacketsGet m_fpPacketsGet = nullptr;
  GHOST_WIN32_WTEnable m_fpEnable = nullptr;
  GHOST_WIN32_WTOverlap m_fpOverlap = nullptr;

  /** Stores the Wintab tablet context. */
  unique_hctx m_context;
  /** Whether the context is enabled. */
  bool m_enabled = false;
  /** Whether the context has focus and is at the top of overlap order. */
  bool m_focused = false;

  /** Pressed button map. */
  uint8_t m_buttons = 0;

  /** Range of a coordinate space. */
  struct Range {
    /** Origin of range. */
    int org = 0;
    /** Extent of range. */
    int ext = 1;
  };

  /** 2D Coordinate space. */
  struct Coord {
    /** Range of x. */
    Range x = {};
    /** Range of y. */
    Range y = {};
  };
  /** Whether Wintab coordinates are trusted. */
  bool m_coordTrusted = false;
  /** Tablet input range. */
  Coord m_tabletCoord = {};
  /** System output range. */
  Coord m_systemCoord = {};

  int m_maxPressure = 0;
  int m_maxAzimuth = 0;
  int m_maxAltitude = 0;

  /** Number of connected Wintab devices. */
  UINT m_numDevices = 0;
  /** Reusable buffer to read in Wintab packets. */
  std::vector<PACKET> m_pkts;
  /** Most recently received tablet data, or none if pen is not in range. */
  GHOST_TabletData m_lastTabletData = GHOST_TABLET_DATA_NONE;

  GHOST_Wintab(unique_hmodule handle,
               GHOST_WIN32_WTInfo info,
               GHOST_WIN32_WTGet get,
               GHOST_WIN32_WTSet set,
               GHOST_WIN32_WTPacketsGet packetsGet,
               GHOST_WIN32_WTEnable enable,
               GHOST_WIN32_WTOverlap overlap,
               unique_hctx hctx,
               Coord tablet,
               Coord system,
               int queueSize);

  /**
   * Convert Wintab system mapped (mouse) buttons into Ghost button mask.
   * \param cursor: The Wintab cursor associated to the button.
   * \param physicalButton: The physical button ID to inspect.
   * \return The system mapped button.
   */
  GHOST_TButtonMask mapWintabToGhostButton(UINT cursor, WORD physicalButton);

  /**
   * Applies common modifications to Wintab context.
   * \param lc: Wintab context to modify.
   */
  static void modifyContext(LOGCONTEXT &lc);

  /**
   * Extracts tablet and system coordinates from Wintab context.
   * \param lc: Wintab context to extract coordinates from.
   * \param tablet: Tablet coordinates.
   * \param system: System coordinates.
   */
  static void extractCoordinates(LOGCONTEXT &lc, Coord &tablet, Coord &system);
};
