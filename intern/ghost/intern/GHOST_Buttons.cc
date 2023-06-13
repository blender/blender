/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup GHOST
 */

#include "GHOST_Buttons.hh"

GHOST_Buttons::GHOST_Buttons()
{
  clear();
}

bool GHOST_Buttons::get(GHOST_TButton mask) const
{
  switch (mask) {
    case GHOST_kButtonMaskLeft:
      return m_ButtonLeft;
    case GHOST_kButtonMaskMiddle:
      return m_ButtonMiddle;
    case GHOST_kButtonMaskRight:
      return m_ButtonRight;
    case GHOST_kButtonMaskButton4:
      return m_Button4;
    case GHOST_kButtonMaskButton5:
      return m_Button5;
    case GHOST_kButtonMaskButton6:
      return m_Button6;
    case GHOST_kButtonMaskButton7:
      return m_Button7;
    default:
      return false;
  }
}

void GHOST_Buttons::set(GHOST_TButton mask, bool down)
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
    case GHOST_kButtonMaskButton4:
      m_Button4 = down;
      break;
    case GHOST_kButtonMaskButton5:
      m_Button5 = down;
      break;
    case GHOST_kButtonMaskButton6:
      m_Button6 = down;
      break;
    case GHOST_kButtonMaskButton7:
      m_Button7 = down;
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
  m_Button4 = false;
  m_Button5 = false;
  m_Button6 = false;
  m_Button7 = false;
}

GHOST_Buttons::~GHOST_Buttons() {}
