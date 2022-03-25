/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_Buttons.h"

GHOST_Buttons::GHOST_Buttons()
{
  clear();
}

bool GHOST_Buttons::get(GHOST_TButtonMask mask) const
{
  switch (mask) {
    case GHOST_kButtonMaskLeft:
      return m_ButtonLeft;
    case GHOST_kButtonMaskMiddle:
      return m_ButtonMiddle;
    case GHOST_kButtonMaskRight:
      return m_ButtonRight;
    default:
      return false;
  }
}

void GHOST_Buttons::set(GHOST_TButtonMask mask, bool down)
{
  switch (mask) {
    case GHOST_kButtonMaskLeft:
      m_ButtonLeft = down;
      break;
    case GHOST_kButtonMaskMiddle:
      m_ButtonMiddle = down;
      break;
    case GHOST_kButtonMaskRight:
      m_ButtonRight = down;
      break;
    default:
      break;
  }
}

void GHOST_Buttons::clear()
{
  m_ButtonLeft = false;
  m_ButtonMiddle = false;
  m_ButtonRight = false;
}

GHOST_Buttons::~GHOST_Buttons()
{
}
