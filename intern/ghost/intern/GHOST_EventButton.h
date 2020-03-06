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
 * Declaration of GHOST_EventButton class.
 */

#ifndef __GHOST_EVENTBUTTON_H__
#define __GHOST_EVENTBUTTON_H__

#include "GHOST_Event.h"
#include "GHOST_Window.h"

/**
 * Mouse button event.
 */
class GHOST_EventButton : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param time      The time this event was generated.
   * \param type      The type of this event.
   * \param window: The window of this event.
   * \param button: The state of the buttons were at the time of the event.
   */
  GHOST_EventButton(GHOST_TUns64 time,
                    GHOST_TEventType type,
                    GHOST_IWindow *window,
                    GHOST_TButtonMask button)
      : GHOST_Event(time, type, window)
  {
    m_buttonEventData.button = button;
    m_buttonEventData.tablet = window->GetTabletData();
    m_data = &m_buttonEventData;
  }

 protected:
  /** The button event data. */
  GHOST_TEventButtonData m_buttonEventData;
};

#endif  // __GHOST_EVENTBUTTON_H__
