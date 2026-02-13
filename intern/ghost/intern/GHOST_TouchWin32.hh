/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of native Windows touchscreen gesture recognizer.
 */

#pragma once

#ifndef WIN32
#  error WIN32 only!
#endif /* WIN32 */

#include <windows.h>

#include <cstdint>
#include <unordered_map>

class GHOST_SystemWin32;
class GHOST_WindowWin32;

class GHOST_TouchWin32 {
 public:
  explicit GHOST_TouchWin32(GHOST_SystemWin32 &system);

  /**
   * Handle native pointer messages for PT_TOUCH.
   * \return True when the event belongs to touchscreen input.
   */
  bool processPointerEvent(UINT type, GHOST_WindowWin32 *window, WPARAM wParam, LPARAM lParam);

  /**
   * Handle pointer leave and clear stale contacts.
   */
  void processPointerLeave(WPARAM wParam);

  /**
   * Drop all active touch state.
   */
  void reset();

 private:
  struct TouchContact {
    POINT screen_pos = {};
    POINT down_screen_pos = {};
    uint64_t down_time_ms = 0;
    bool moved = false;
  };

  struct TouchFrame {
    int finger_count = 0;
    POINT centroid = {};
    float finger_distance = 0.0f;
  };

  bool pointerInfoFromWParam(WPARAM wParam, POINTER_INFO &r_pointer_info) const;
  uint64_t pointerTimeMs(const POINTER_INFO &pointer_info) const;

  void processPointerDown(GHOST_WindowWin32 *window, const POINTER_INFO &pointer_info);
  void processPointerUpdate(GHOST_WindowWin32 *window, const POINTER_INFO &pointer_info);
  void processPointerUp(GHOST_WindowWin32 *window, const POINTER_INFO &pointer_info);

  bool shouldSynthesizeTap(const TouchContact &contact, uint64_t time_ms) const;
  void synthesizeTap(GHOST_WindowWin32 *window, const POINT &screen_pos, uint64_t time_ms);

  TouchFrame frameFromContacts() const;
  void resetGestureBaseline();
  void setGestureBaseline(const TouchFrame &frame);
  void emitGestureEvents(GHOST_WindowWin32 *window, uint64_t time_ms);

  static int distanceSquared(const POINT &a, const POINT &b);

  GHOST_SystemWin32 &system_;
  std::unordered_map<UINT32, TouchContact> contacts_;

  bool sequence_had_multitouch_ = false;
  bool gesture_baseline_valid_ = false;
  int gesture_baseline_fingers_ = 0;
  POINT gesture_baseline_centroid_ = {};
  float gesture_baseline_distance_ = 0.0f;
};

