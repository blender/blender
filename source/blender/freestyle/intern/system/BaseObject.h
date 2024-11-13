/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Base Class for most shared objects (Node, Rep). Defines the addRef, release system.
 * \brief Inspired by COM IUnknown system.
 */

#include "MEM_guardedalloc.h"

#include "BLI_sys_types.h"

namespace Freestyle {

class BaseObject {
 public:
  inline BaseObject()
  {
    _ref_counter = 0;
  }

  virtual ~BaseObject() {}

  /** At least makes a release on this.
   *  The BaseObject::destroy method must be explicitly called at the end of any overloaded destroy
   */
  virtual int destroy()
  {
    return release();
  }

  /** Increments the reference counter */
  inline int addRef()
  {
    return ++_ref_counter;
  }

  /** Decrements the reference counter */
  inline int release()
  {
    if (_ref_counter) {
      _ref_counter--;
    }
    return _ref_counter;
  }

 private:
  uint _ref_counter;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BaseObject")
};

} /* namespace Freestyle */
