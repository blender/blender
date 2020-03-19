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

#ifndef __BLI_MEMORY_UTILS_CXX_H__
#define __BLI_MEMORY_UTILS_CXX_H__

/** \file
 * \ingroup bli
 */

#include <algorithm>
#include <memory>

#include "BLI_utildefines.h"

namespace BLI {

using std::copy;
using std::copy_n;
using std::uninitialized_copy;
using std::uninitialized_copy_n;
using std::uninitialized_fill;
using std::uninitialized_fill_n;

template<typename T> void construct_default(T *ptr)
{
  new (ptr) T();
}

template<typename T> void destruct(T *ptr)
{
  ptr->~T();
}

template<typename T> void destruct_n(T *ptr, uint n)
{
  for (uint i = 0; i < n; i++) {
    ptr[i].~T();
  }
}

template<typename T> void uninitialized_move_n(T *src, uint n, T *dst)
{
  std::uninitialized_copy_n(std::make_move_iterator(src), n, dst);
}

template<typename T> void move_n(T *src, uint n, T *dst)
{
  std::copy_n(std::make_move_iterator(src), n, dst);
}

template<typename T> void uninitialized_relocate(T *src, T *dst)
{
  new (dst) T(std::move(*src));
  destruct(src);
}

template<typename T> void uninitialized_relocate_n(T *src, uint n, T *dst)
{
  uninitialized_move_n(src, n, dst);
  destruct_n(src, n);
}

template<typename T> void relocate(T *src, T *dst)
{
  *dst = std::move(*src);
  destruct(src);
}

template<typename T> void relocate_n(T *src, uint n, T *dst)
{
  move_n(src, n, dst);
  destruct_n(src, n);
}

template<typename T, typename... Args> std::unique_ptr<T> make_unique(Args &&... args)
{
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<typename T> struct DestructValueAtAddress {
  void operator()(T *ptr)
  {
    ptr->~T();
  }
};

template<typename T> using destruct_ptr = std::unique_ptr<T, DestructValueAtAddress<T>>;

template<uint Size, uint Alignment> class alignas(Alignment) AlignedBuffer {
 private:
  /* Don't create an empty array. This causes problems with some compilers. */
  static constexpr uint ActualSize = (Size > 0) ? Size : 1;
  char m_buffer[ActualSize];

 public:
  void *ptr()
  {
    return (void *)m_buffer;
  }

  const void *ptr() const
  {
    return (const void *)m_buffer;
  }
};

}  // namespace BLI

#endif /* __BLI_MEMORY_UTILS_CXX_H__ */
