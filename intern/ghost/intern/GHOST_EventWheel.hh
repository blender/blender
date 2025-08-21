/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventWheel class.
 */

#pragma once

#include "GHOST_Event.hh"

/**
 * Mouse wheel event.
 * The displacement of the mouse wheel is counted in ticks.
 * A positive value means the wheel is turned away from the user.
 */
class GHOST_EventWheel : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param msec: The time this event was generated.
   * \param window: The window of this event.
   * \param axis: The axis of the mouse wheel.
   * \param value: The displacement of the mouse wheel.
   */
  GHOST_EventWheel(uint64_t msec, GHOST_IWindow *window, GHOST_TEventWheelAxis axis, int32_t value)
      : GHOST_Event(msec, GHOST_kEventWheel, window)
  {
    wheel_event_data_.axis = axis;
    wheel_event_data_.value = value;
    data_ = &wheel_event_data_;
  }

 protected:
  GHOST_TEventWheelData wheel_event_data_;
};
