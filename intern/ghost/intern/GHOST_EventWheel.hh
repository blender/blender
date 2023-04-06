/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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
   * \param z: The displacement of the mouse wheel.
   */
  GHOST_EventWheel(uint64_t msec, GHOST_IWindow *window, int32_t z)
      : GHOST_Event(msec, GHOST_kEventWheel, window)
  {
    m_wheelEventData.z = z;
    m_data = &m_wheelEventData;
  }

 protected:
  /** The z-displacement of the mouse wheel. */
  GHOST_TEventWheelData m_wheelEventData;
};
