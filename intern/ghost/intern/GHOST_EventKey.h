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
 * Declaration of GHOST_EventKey class.
 */

#ifndef __GHOST_EVENTKEY_H__
#define __GHOST_EVENTKEY_H__

#include "GHOST_Event.h"

/**
 * Key event.
 */
class GHOST_EventKey : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param msec  The time this event was generated.
   * \param type  The type of key event.
   * \param key   The key code of the key.
   */
  GHOST_EventKey(GHOST_TUns64 msec, GHOST_TEventType type, GHOST_IWindow *window, GHOST_TKey key)
      : GHOST_Event(msec, type, window)
  {
    m_keyEventData.key = key;
    m_keyEventData.ascii = '\0';
    m_keyEventData.utf8_buf[0] = '\0';
    m_data = &m_keyEventData;
  }

  /**
   * Constructor.
   * \param msec  The time this event was generated.
   * \param type  The type of key event.
   * \param key   The key code of the key.
   * \param ascii The ascii code for the key event.
   */
  GHOST_EventKey(GHOST_TUns64 msec,
                 GHOST_TEventType type,
                 GHOST_IWindow *window,
                 GHOST_TKey key,
                 char ascii,
                 const char utf8_buf[6])
      : GHOST_Event(msec, type, window)
  {
    m_keyEventData.key = key;
    m_keyEventData.ascii = ascii;
    if (utf8_buf) {
      memcpy(m_keyEventData.utf8_buf, utf8_buf, sizeof(m_keyEventData.utf8_buf));
    }
    else {
      m_keyEventData.utf8_buf[0] = '\0';
    }
    m_data = &m_keyEventData;
  }

 protected:
  /** The key event data. */
  GHOST_TEventKeyData m_keyEventData;
};

#endif  // __GHOST_EVENTKEY_H__
