// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2011 Benoit Jacob <jacob.benoit.1@gmail.com>
//
// Eigen is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 3 of the License, or (at your option) any later version.
//
// Alternatively, you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// Eigen is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License or the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License and a copy of the GNU General Public License along with
// Eigen. If not, see <http://www.gnu.org/licenses/>.

#ifndef EIGEN2_MEMORY_H
#define EIGEN2_MEMORY_H

inline void* ei_aligned_malloc(size_t size) { return internal::aligned_malloc(size); }
inline void  ei_aligned_free(void *ptr) { internal::aligned_free(ptr); }
inline void* ei_aligned_realloc(void *ptr, size_t new_size, size_t old_size) { return internal::aligned_realloc(ptr, new_size, old_size); }
inline void* ei_handmade_aligned_malloc(size_t size) { return internal::handmade_aligned_malloc(size); }
inline void  ei_handmade_aligned_free(void *ptr) { internal::handmade_aligned_free(ptr); }

template<bool Align> inline void* ei_conditional_aligned_malloc(size_t size)
{
  return internal::conditional_aligned_malloc<Align>(size);
}
template<bool Align> inline void ei_conditional_aligned_free(void *ptr)
{
  internal::conditional_aligned_free<Align>(ptr);
}
template<bool Align> inline void* ei_conditional_aligned_realloc(void* ptr, size_t new_size, size_t old_size)
{
  return internal::conditional_aligned_realloc<Align>(ptr, new_size, old_size);
}

template<typename T> inline T* ei_aligned_new(size_t size)
{
  return internal::aligned_new<T>(size);
}
template<typename T> inline void ei_aligned_delete(T *ptr, size_t size)
{
  return internal::aligned_delete(ptr, size);
}



#endif // EIGEN2_MACROS_H
