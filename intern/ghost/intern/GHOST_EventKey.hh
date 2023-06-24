/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventKey class.
 */

#pragma once

#include <string.h>

#include "GHOST_Event.hh"

/**
 * Key event.
 */
class GHOST_EventKey : public GHOST_Event {
 public:
  /**
   * Constructor.
   * \param msec: The time this event was generated.
   * \param type: The type of key event.
   * \param key: The key code of the key.
   * \param is_repeat: Enabled for key repeat events (only for press events).
   */
  GHOST_EventKey(
      uint64_t msec, GHOST_TEventType type, GHOST_IWindow *window, GHOST_TKey key, bool is_repeat)
      : GHOST_Event(msec, type, window)
  {
    m_keyEventData.key = key;
    m_keyEventData.utf8_buf[0] = '\0';
    m_keyEventData.is_repeat = is_repeat;
    m_data = &m_keyEventData;
  }

  /**
   * Constructor.
   * \param msec: The time this event was generated.
   * \param type: The type of key event.
   * \param key: The key code of the key.
   * \param is_repeat: Enabled for key repeat events (only for press events).
   * \param utf8_buf: The text associated with this key event (only for press events).
   */
  GHOST_EventKey(uint64_t msec,
                 GHOST_TEventType type,
                 GHOST_IWindow *window,
                 GHOST_TKey key,
                 bool is_repeat,
                 const char utf8_buf[6])
      : GHOST_Event(msec, type, window)
  {
    m_keyEventData.key = key;
    if (utf8_buf) {
      memcpy(m_keyEventData.utf8_buf, utf8_buf, sizeof(m_keyEventData.utf8_buf));
    }
    else {
      m_keyEventData.utf8_buf[0] = '\0';
    }
    m_keyEventData.is_repeat = is_repeat;
    m_data = &m_keyEventData;
  }

 protected:
  /** The key event data. */
  GHOST_TEventKeyData m_keyEventData;
};
