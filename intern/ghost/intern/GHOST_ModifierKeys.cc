/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 */

/**
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#include "GHOST_ModifierKeys.hh"
#include "GHOST_Debug.hh"

GHOST_ModifierKeys::GHOST_ModifierKeys()
{
  clear();
}

GHOST_ModifierKeys::~GHOST_ModifierKeys() = default;

GHOST_TKey GHOST_ModifierKeys::getModifierKeyCode(GHOST_TModifierKey mask)
{
  GHOST_TKey key;
  switch (mask) {
    case GHOST_kModifierKeyLeftShift:
      key = GHOST_kKeyLeftShift;
      break;
    case GHOST_kModifierKeyRightShift:
      key = GHOST_kKeyRightShift;
      break;
    case GHOST_kModifierKeyLeftAlt:
      key = GHOST_kKeyLeftAlt;
      break;
    case GHOST_kModifierKeyRightAlt:
      key = GHOST_kKeyRightAlt;
      break;
    case GHOST_kModifierKeyLeftControl:
      key = GHOST_kKeyLeftControl;
      break;
    case GHOST_kModifierKeyRightControl:
      key = GHOST_kKeyRightControl;
      break;
    case GHOST_kModifierKeyLeftOS:
      key = GHOST_kKeyLeftOS;
      break;
    case GHOST_kModifierKeyRightOS:
      key = GHOST_kKeyRightOS;
      break;
    case GHOST_kModifierKeyLeftHyper:
      key = GHOST_kKeyLeftHyper;
      break;
    case GHOST_kModifierKeyRightHyper:
      key = GHOST_kKeyRightHyper;
      break;
    default:
      /* Should not happen. */
      GHOST_ASSERT(0, "Invalid key!");
      key = GHOST_kKeyUnknown;
      break;
  }
  return key;
}

bool GHOST_ModifierKeys::get(GHOST_TModifierKey mask) const
{
  switch (mask) {
    case GHOST_kModifierKeyLeftShift:
      return left_shift_;
    case GHOST_kModifierKeyRightShift:
      return right_shift_;
    case GHOST_kModifierKeyLeftAlt:
      return left_alt_;
    case GHOST_kModifierKeyRightAlt:
      return right_alt_;
    case GHOST_kModifierKeyLeftControl:
      return left_control_;
    case GHOST_kModifierKeyRightControl:
      return right_control_;
    case GHOST_kModifierKeyLeftOS:
      return left_os_;
    case GHOST_kModifierKeyRightOS:
      return right_os_;
    case GHOST_kModifierKeyLeftHyper:
      return left_hyper_;
    case GHOST_kModifierKeyRightHyper:
      return right_hyper_;
    default:
      GHOST_ASSERT(0, "Invalid key!");
      return false;
  }
}

void GHOST_ModifierKeys::set(GHOST_TModifierKey mask, bool down)
{
  switch (mask) {
    case GHOST_kModifierKeyLeftShift:
      left_shift_ = down;
      break;
    case GHOST_kModifierKeyRightShift:
      right_shift_ = down;
      break;
    case GHOST_kModifierKeyLeftAlt:
      left_alt_ = down;
      break;
    case GHOST_kModifierKeyRightAlt:
      right_alt_ = down;
      break;
    case GHOST_kModifierKeyLeftControl:
      left_control_ = down;
      break;
    case GHOST_kModifierKeyRightControl:
      right_control_ = down;
      break;
    case GHOST_kModifierKeyLeftOS:
      left_os_ = down;
      break;
    case GHOST_kModifierKeyRightOS:
      right_os_ = down;
      break;
    case GHOST_kModifierKeyLeftHyper:
      left_hyper_ = down;
      break;
    case GHOST_kModifierKeyRightHyper:
      right_hyper_ = down;
      break;
    default:
      GHOST_ASSERT(0, "Invalid key!");
      break;
  }
}

void GHOST_ModifierKeys::clear()
{
  left_shift_ = false;
  right_shift_ = false;
  left_alt_ = false;
  right_alt_ = false;
  left_control_ = false;
  right_control_ = false;
  left_os_ = false;
  right_os_ = false;
  left_hyper_ = false;
  right_hyper_ = false;
}

bool GHOST_ModifierKeys::equals(const GHOST_ModifierKeys &keys) const
{
  return (left_shift_ == keys.left_shift_) && (right_shift_ == keys.right_shift_) &&
         (left_alt_ == keys.left_alt_) && (right_alt_ == keys.right_alt_) &&
         (left_control_ == keys.left_control_) && (right_control_ == keys.right_control_) &&
         (left_os_ == keys.left_os_) && (right_os_ == keys.right_os_) &&
         (left_hyper_ == keys.left_hyper_) && (right_hyper_ == keys.right_hyper_);
}
