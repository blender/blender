/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
      return button_left_;
    case GHOST_kButtonMaskMiddle:
      return button_middle_;
    case GHOST_kButtonMaskRight:
      return button_right_;
    case GHOST_kButtonMaskButton4:
      return button4_;
    case GHOST_kButtonMaskButton5:
      return button5_;
    case GHOST_kButtonMaskButton6:
      return button6_;
    case GHOST_kButtonMaskButton7:
      return button7_;
    default:
      return false;
  }
}

void GHOST_Buttons::set(GHOST_TButton mask, bool down)
{
  switch (mask) {
    case GHOST_kButtonMaskLeft:
      button_left_ = down;
      break;
    case GHOST_kButtonMaskMiddle:
      button_middle_ = down;
      break;
    case GHOST_kButtonMaskRight:
      button_right_ = down;
      break;
    case GHOST_kButtonMaskButton4:
      button4_ = down;
      break;
    case GHOST_kButtonMaskButton5:
      button5_ = down;
      break;
    case GHOST_kButtonMaskButton6:
      button6_ = down;
      break;
    case GHOST_kButtonMaskButton7:
      button7_ = down;
      break;
    default:
      break;
  }
}

void GHOST_Buttons::clear()
{
  button_left_ = false;
  button_middle_ = false;
  button_right_ = false;
  button4_ = false;
  button5_ = false;
  button6_ = false;
  button7_ = false;
}

GHOST_Buttons::~GHOST_Buttons() = default;
