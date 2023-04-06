/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventCursor class.
 */

#pragma once

#include "GHOST_Event.hh"

/**
 * Cursor event.
 */
class GHOST_EventCursor : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param msec: The time this event was generated.
   * \param type: The type of this event.
   * \param x: The x-coordinate of the location the cursor was at the time of the event.
   * \param y: The y-coordinate of the location the cursor was at the time of the event.
   * \param tablet: The tablet data associated with this event.
   */
  GHOST_EventCursor(uint64_t msec,
                    GHOST_TEventType type,
                    GHOST_IWindow *window,
                    int32_t x,
                    int32_t y,
                    const GHOST_TabletData &tablet)
      : GHOST_Event(msec, type, window), m_cursorEventData({x, y, tablet})
  {
    m_data = &m_cursorEventData;
  }

 protected:
  /** The x,y-coordinates of the cursor position. */
  GHOST_TEventCursorData m_cursorEventData;
};
