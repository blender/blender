/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_EventKey class.
 */

#pragma once

#include <cstring>

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
    key_event_data_.key = key;
    key_event_data_.utf8_buf[0] = '\0';
    key_event_data_.is_repeat = is_repeat;
    data_ = &key_event_data_;
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
    key_event_data_.key = key;
    if (utf8_buf) {
      memcpy(key_event_data_.utf8_buf, utf8_buf, sizeof(key_event_data_.utf8_buf));
    }
    else {
      key_event_data_.utf8_buf[0] = '\0';
    }
    key_event_data_.is_repeat = is_repeat;
    data_ = &key_event_data_;
  }

 protected:
  /** The key event data. */
  GHOST_TEventKeyData key_event_data_;
};
