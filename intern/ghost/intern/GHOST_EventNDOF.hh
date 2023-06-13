/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

#pragma once

#ifndef WITH_INPUT_NDOF
#  error NDOF code included in non-NDOF-enabled build
#endif

#include "GHOST_Event.hh"

class GHOST_EventNDOFMotion : public GHOST_Event {
 protected:
  GHOST_TEventNDOFMotionData m_axisData;

 public:
  GHOST_EventNDOFMotion(uint64_t time, GHOST_IWindow *window)
      : GHOST_Event(time, GHOST_kEventNDOFMotion, window)
  {
    m_data = &m_axisData;
  }
};

class GHOST_EventNDOFButton : public GHOST_Event {
 protected:
  GHOST_TEventNDOFButtonData m_buttonData;

 public:
  GHOST_EventNDOFButton(uint64_t time, GHOST_IWindow *window)
      : GHOST_Event(time, GHOST_kEventNDOFButton, window)
  {
    m_data = &m_buttonData;
  }
};
