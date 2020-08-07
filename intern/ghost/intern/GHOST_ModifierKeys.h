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
 * Declaration of GHOST_ModifierKeys struct.
 */

#pragma once

#include "GHOST_Types.h"

/**
 * Stores the state of modifier keys.
 * Discriminates between left and right modifier keys.
 */
struct GHOST_ModifierKeys {
  /**
   * Constructor.
   */
  GHOST_ModifierKeys();

  ~GHOST_ModifierKeys();

  /**
   * Returns the modifier key's key code from a modifier key mask.
   * \param mask The mask of the modifier key.
   * \return The modifier key's key code.
   */
  static GHOST_TKey getModifierKeyCode(GHOST_TModifierKeyMask mask);

  /**
   * Returns the state of a single modifier key.
   * \param mask: Key state to return.
   * \return The state of the key (pressed == true).
   */
  bool get(GHOST_TModifierKeyMask mask) const;

  /**
   * Updates the state of a single modifier key.
   * \param mask: Key state to update.
   * \param down: The new state of the key.
   */
  void set(GHOST_TModifierKeyMask mask, bool down);

  /**
   * Sets the state of all modifier keys to up.
   */
  void clear();

  /**
   * Determines whether to modifier key states are equal.
   * \param keys: The modifier key state to compare to.
   * \return Indication of equality.
   */
  bool equals(const GHOST_ModifierKeys &keys) const;

  /** Bitfield that stores the appropriate key state. */
  GHOST_TUns8 m_LeftShift : 1;
  /** Bitfield that stores the appropriate key state. */
  GHOST_TUns8 m_RightShift : 1;
  /** Bitfield that stores the appropriate key state. */
  GHOST_TUns8 m_LeftAlt : 1;
  /** Bitfield that stores the appropriate key state. */
  GHOST_TUns8 m_RightAlt : 1;
  /** Bitfield that stores the appropriate key state. */
  GHOST_TUns8 m_LeftControl : 1;
  /** Bitfield that stores the appropriate key state. */
  GHOST_TUns8 m_RightControl : 1;
  /** Bitfield that stores the appropriate key state. */
  GHOST_TUns8 m_OS : 1;
};
