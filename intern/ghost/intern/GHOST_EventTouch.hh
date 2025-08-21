/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventTouch class.
 */

#pragma once

#include "GHOST_Event.hh"

/**
 * Touch event.
 */
class GHOST_EventTouch : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param msec: The time this event was generated.
   * \param window: The window of this event.
   * \param subtype: The subtype of the event.
   * \param x: The x-delta of the pan event.
   * \param y: The y-delta of the pan event.
   */
  GHOST_EventTouch(uint64_t msec,
                   GHOST_IWindow *window,
                   GHOST_TTouchEventSubTypes subtype,
                   int32_t x,
                   int32_t y,
                   uint numFingers = 1)
      : GHOST_Event(msec, GHOST_kEventTouch, window)
  {
    touch_event_data_.subtype = subtype;
    touch_event_data_.x = x;
    touch_event_data_.y = y;
    touch_event_data_.numFingers = numFingers;
    data_ = &touch_event_data_;
  }

 protected:
  GHOST_TEventTouchData touch_event_data_;
};
