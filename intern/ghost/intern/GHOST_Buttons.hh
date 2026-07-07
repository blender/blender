/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 * Declaration of GHOST_Buttons struct.
 */

#pragma once

#include "GHOST_Types.hh"

/**
 * This struct stores the state of the mouse buttons.
 * Buttons can be set using button masks.
 */
struct GHOST_Buttons {
  /**
   * Constructor.
   */
  GHOST_Buttons();

  ~GHOST_Buttons();

  /**
   * Returns the state of a single button.
   * \param mask: Key button to return.
   * \return The state of the button (pressed == true).
   */
  bool get(GHOST_TButton mask) const;

  /**
   * Updates the state of a single button.
   * \param mask: Button state to update.
   * \param down: The new state of the button.
   */
  void set(GHOST_TButton mask, bool down);

  /**
   * Sets the state of all buttons to up.
   */
  void clear();

  uint8_t button_left_ : 1;
  uint8_t button_middle_ : 1;
  uint8_t button_right_ : 1;
  uint8_t button4_ : 1;
  uint8_t button5_ : 1;
  uint8_t button6_ : 1;
  uint8_t button7_ : 1;
};
