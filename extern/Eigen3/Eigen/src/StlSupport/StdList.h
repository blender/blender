// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
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

#ifndef EIGEN_STDLIST_H
#define EIGEN_STDLIST_H

#include "Eigen/src/StlSupport/details.h"

// Define the explicit instantiation (e.g. necessary for the Intel compiler)
#if defined(__INTEL_COMPILER) || defined(__GNUC__)
  #define EIGEN_EXPLICIT_STL_LIST_INSTANTIATION(...) template class std::list<__VA_ARGS__, EIGEN_ALIGNED_ALLOCATOR<__VA_ARGS__> >;
#else
  #define EIGEN_EXPLICIT_STL_LIST_INSTANTIATION(...)
#endif

/**
 * This section contains a convenience MACRO which allows an easy specialization of
 * std::list such that for data types with alignment issues the correct allocator
 * is used automatically.
 */
#define EIGEN_DEFINE_STL_LIST_SPECIALIZATION(...) \
EIGEN_EXPLICIT_STL_LIST_INSTANTIATION(__VA_ARGS__) \
namespace std \
{ \
  template<typename _Ay> \
  class list<__VA_ARGS__, _Ay>  \
    : public list<__VA_ARGS__, EIGEN_ALIGNED_ALLOCATOR<__VA_ARGS__> > \
  { \
    typedef list<__VA_ARGS__, EIGEN_ALIGNED_ALLOCATOR<__VA_ARGS__> > list_base; \
  public: \
    typedef __VA_ARGS__ value_type; \
    typedef typename list_base::allocator_type allocator_type; \
    typedef typename list_base::size_type size_type;  \
    typedef typename list_base::iterator iterator;  \
    explicit list(const allocator_type& a = allocator_type()) : list_base(a) {}  \
    template<typename InputIterator> \
    list(InputIterator first, InputIterator last, const allocator_type& a = allocator_type()) : list_base(first, last, a) {} \
    list(const list& c) : list_base(c) {}  \
    explicit list(size_type num, const value_type& val = value_type()) : list_base(num, val) {} \
    list(iterator start, iterator end) : list_base(start, end) {}  \
    list& operator=(const list& x) {  \
      list_base::operator=(x);  \
      return *this;  \
    } \
  }; \
}

// check whether we really need the std::vector specialization
#if !(defined(_GLIBCXX_VECTOR) && (!EIGEN_GNUC_AT_LEAST(4,1))) /* Note that before gcc-4.1 we already have: std::list::resize(size_type,const T&). */

namespace std
{

#define EIGEN_STD_LIST_SPECIALIZATION_BODY \
  public:  \
    typedef T value_type; \
    typedef typename list_base::allocator_type allocator_type; \
    typedef typename list_base::size_type size_type;  \
    typedef typename list_base::iterator iterator;  \
    typedef typename list_base::const_iterator const_iterator;  \
    explicit list(const allocator_type& a = allocator_type()) : list_base(a) {}  \
    template<typename InputIterator> \
    list(InputIterator first, InputIterator last, const allocator_type& a = allocator_type()) \
    : list_base(first, last, a) {} \
    list(const list& c) : list_base(c) {}  \
    explicit list(size_type num, const value_type& val = value_type()) : list_base(num, val) {} \
    list(iterator start, iterator end) : list_base(start, end) {}  \
    list& operator=(const list& x) {  \
    list_base::operator=(x);  \
    return *this; \
  }

  template<typename T>
  class list<T,EIGEN_ALIGNED_ALLOCATOR<T> >
    : public list<EIGEN_WORKAROUND_MSVC_STL_SUPPORT(T),
                  Eigen::aligned_allocator_indirection<EIGEN_WORKAROUND_MSVC_STL_SUPPORT(T)> >
  {
    typedef list<EIGEN_WORKAROUND_MSVC_STL_SUPPORT(T),
                 Eigen::aligned_allocator_indirection<EIGEN_WORKAROUND_MSVC_STL_SUPPORT(T)> > list_base;
    EIGEN_STD_LIST_SPECIALIZATION_BODY

    void resize(size_type new_size)
    { resize(new_size, T()); }

    void resize(size_type new_size, const value_type& x)
    {
      if (list_base::size() < new_size)
        list_base::insert(list_base::end(), new_size - list_base::size(), x);
      else
        while (new_size < list_base::size()) list_base::pop_back();
    }

#if defined(_LIST_)
    // workaround MSVC std::list implementation
    void push_back(const value_type& x)
    { list_base::push_back(x); } 
    using list_base::insert;  
    iterator insert(const_iterator position, const value_type& x)
    { return list_base::insert(position,x); }
    void insert(const_iterator position, size_type new_size, const value_type& x)
    { list_base::insert(position, new_size, x); }
#endif
  };
}

#endif // check whether specialization is actually required

#endif // EIGEN_STDLIST_H
