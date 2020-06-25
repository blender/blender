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

#ifndef __FREESTYLE_AUTOPTR_HELPER_H__
#define __FREESTYLE_AUTOPTR_HELPER_H__

/** \file
 * \ingroup freestyle
 * \brief Utility header for auto_ptr/unique_ptr selection
 */

#include <memory>

namespace Freestyle {

template<typename T> class AutoPtr : public std::unique_ptr<T> {
 public:
  AutoPtr() : std::unique_ptr<T>()
  {
  }
  AutoPtr(T *ptr) : std::unique_ptr<T>(ptr)
  {
  }

  /* Mimic behavior of legacy auto_ptr.
   * Keep implementation as small as possible, hens delete assignment operator. */

  template<typename X> AutoPtr(AutoPtr<X> &other) : std::unique_ptr<T>(other.get())
  {
    other.release();
  }

  template<typename X> AutoPtr &operator=(AutoPtr<X> &other) = delete;
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_AUTOPTR_HELPER_H__
