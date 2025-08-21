/* SPDX-FileCopyrightText: 2002-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  GHOST_TEventNDOFMotionData axis_data_;

 public:
  GHOST_EventNDOFMotion(uint64_t time, GHOST_IWindow *window)
      : GHOST_Event(time, GHOST_kEventNDOFMotion, window)
  {
    data_ = &axis_data_;
  }
};

class GHOST_EventNDOFButton : public GHOST_Event {
 protected:
  GHOST_TEventNDOFButtonData button_data_;

 public:
  GHOST_EventNDOFButton(uint64_t time, GHOST_IWindow *window)
      : GHOST_Event(time, GHOST_kEventNDOFButton, window)
  {
    data_ = &button_data_;
  }
};
