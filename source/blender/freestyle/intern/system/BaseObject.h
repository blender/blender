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
 */

#ifndef __FREESTYLE_BASE_OBJECT_H__
#define __FREESTYLE_BASE_OBJECT_H__

/** \file
 * \ingroup freestyle
 * \brief Base Class for most shared objects (Node, Rep). Defines the addRef, release system.
 * \brief Inspired by COM IUnknown system.
 */

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class BaseObject {
 public:
  inline BaseObject()
  {
    _ref_counter = 0;
  }

  virtual ~BaseObject()
  {
  }

  /*! At least makes a release on this.
   *  The BaseObject::destroy method must be explicitly called at the end of any overloaded destroy
   */
  virtual int destroy()
  {
    return release();
  }

  /*! Increments the reference counter */
  inline int addRef()
  {
    return ++_ref_counter;
  }

  /*! Decrements the reference counter */
  inline int release()
  {
    if (_ref_counter) {
      _ref_counter--;
    }
    return _ref_counter;
  }

 private:
  unsigned _ref_counter;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BaseObject")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_BASE_OBJECT_H__
