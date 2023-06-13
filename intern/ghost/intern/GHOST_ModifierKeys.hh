/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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
   * \param mask: The mask of the modifier key.
   * \return The modifier key's key code.
   */
  static GHOST_TKey getModifierKeyCode(GHOST_TModifierKey mask);

  /**
   * Returns the state of a single modifier key.
   * \param mask: Key state to return.
   * \return The state of the key (pressed == true).
   */
  bool get(GHOST_TModifierKey mask) const;

  /**
   * Updates the state of a single modifier key.
   * \param mask: Key state to update.
   * \param down: The new state of the key.
   */
  void set(GHOST_TModifierKey mask, bool down);

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

  /** Bit-field that stores the appropriate key state. */
  uint8_t m_LeftShift : 1;
  /** Bit-field that stores the appropriate key state. */
  uint8_t m_RightShift : 1;
  /** Bit-field that stores the appropriate key state. */
  uint8_t m_LeftAlt : 1;
  /** Bit-field that stores the appropriate key state. */
  uint8_t m_RightAlt : 1;
  /** Bit-field that stores the appropriate key state. */
  uint8_t m_LeftControl : 1;
  /** Bit-field that stores the appropriate key state. */
  uint8_t m_RightControl : 1;
  /** Bit-field that stores the appropriate key state. */
  uint8_t m_LeftOS : 1;
  uint8_t m_RightOS : 1;
};
