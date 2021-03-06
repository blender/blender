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
 */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventString class.
 */

#pragma once

#include "GHOST_Event.h"

/**
 * Generic class for events with string data
 */
class GHOST_EventString : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param msec: The time this event was generated.
   * \param type: The type of this event.
   * \param window: The generating window (or NULL if system event).
   * \param data_ptr: Pointer to the (un-formatted) data associated with the event.
   */
  GHOST_EventString(uint64_t msec,
                    GHOST_TEventType type,
                    GHOST_IWindow *window,
                    GHOST_TEventDataPtr data_ptr)
      : GHOST_Event(msec, type, window)
  {
    m_data = data_ptr;
  }

  ~GHOST_EventString()
  {
    if (m_data)
      free(m_data);
  }
};
