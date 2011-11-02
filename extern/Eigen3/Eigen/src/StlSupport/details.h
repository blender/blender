// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2009 Gael Guennebaud <gael.guennebaud@inria.fr>
// Copyright (C) 2009 Hauke Heibel <hauke.heibel@googlemail.com>
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

#ifndef EIGEN_STL_DETAILS_H
#define EIGEN_STL_DETAILS_H

#ifndef EIGEN_ALIGNED_ALLOCATOR
  #define EIGEN_ALIGNED_ALLOCATOR Eigen::aligned_allocator
#endif

namespace Eigen {

  // This one is needed to prevent reimplementing the whole std::vector.
  template <class T>
  class aligned_allocator_indirection : public EIGEN_ALIGNED_ALLOCATOR<T>
  {
  public:
    typedef size_t    size_type;
    typedef ptrdiff_t difference_type;
    typedef T*        pointer;
    typedef const T*  const_pointer;
    typedef T&        reference;
    typedef const T&  const_reference;
    typedef T         value_type;

    template<class U>
    struct rebind
    {
      typedef aligned_allocator_indirection<U> other;
    };

    aligned_allocator_indirection() {}
    aligned_allocator_indirection(const aligned_allocator_indirection& ) : EIGEN_ALIGNED_ALLOCATOR<T>() {}
    aligned_allocator_indirection(const EIGEN_ALIGNED_ALLOCATOR<T>& ) {}
    template<class U>
    aligned_allocator_indirection(const aligned_allocator_indirection<U>& ) {}
    template<class U>
    aligned_allocator_indirection(const EIGEN_ALIGNED_ALLOCATOR<U>& ) {}
    ~aligned_allocator_indirection() {}
  };

#ifdef _MSC_VER

  // sometimes, MSVC detects, at compile time, that the argument x
  // in std::vector::resize(size_t s,T x) won't be aligned and generate an error
  // even if this function is never called. Whence this little wrapper.
#define EIGEN_WORKAROUND_MSVC_STL_SUPPORT(T) \
  typename Eigen::internal::conditional< \
    Eigen::internal::is_arithmetic<T>::value, \
    T, \
    Eigen::internal::workaround_msvc_stl_support<T> \
  >::type

  namespace internal {
  template<typename T> struct workaround_msvc_stl_support : public T
  {
    inline workaround_msvc_stl_support() : T() {}
    inline workaround_msvc_stl_support(const T& other) : T(other) {}
    inline operator T& () { return *static_cast<T*>(this); }
    inline operator const T& () const { return *static_cast<const T*>(this); }
    template<typename OtherT>
    inline T& operator=(const OtherT& other)
    { T::operator=(other); return *this; }
    inline workaround_msvc_stl_support& operator=(const workaround_msvc_stl_support& other)
    { T::operator=(other); return *this; }
  };
  }

#else

#define EIGEN_WORKAROUND_MSVC_STL_SUPPORT(T) T

#endif

}

#endif // EIGEN_STL_DETAILS_H
