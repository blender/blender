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

#ifndef __FREESTYLE_BASE_ITERATOR_H__
#define __FREESTYLE_BASE_ITERATOR_H__

/** \file
 * \ingroup freestyle
 * \brief Classes defining the basic "Iterator" design pattern
 */

#include <iterator>

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

// use for iterators definitions
template<class Element> class Nonconst_traits;

template<class Element> class Const_traits {
 public:
  typedef Element value_type;
  typedef const Element &reference;
  typedef const Element *pointer;
  typedef ptrdiff_t difference_type;
  typedef Nonconst_traits<Element> Non_const_traits;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Const_traits")
#endif
};

template<class Element> class Nonconst_traits {
 public:
  typedef Element value_type;
  typedef Element &reference;
  typedef Element *pointer;
  typedef ptrdiff_t difference_type;
  typedef Nonconst_traits<Element> Non_const_traits;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Nonconst_traits")
#endif
};

class InputIteratorTag_Traits {
 public:
  typedef std::input_iterator_tag iterator_category;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:InputIteratorTag_Traits")
#endif
};

class BidirectionalIteratorTag_Traits {
 public:
  typedef std::bidirectional_iterator_tag iterator_category;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BidirectionalIteratorTag_Traits")
#endif
};

template<class Traits, class IteratorTagTraits> class IteratorBase {
 public:
  virtual ~IteratorBase()
  {
  }

  virtual bool begin() const = 0;
  virtual bool end() const = 0;

  typedef typename IteratorTagTraits::iterator_category iterator_category;
  typedef typename Traits::value_type value_type;
  typedef typename Traits::difference_type difference_type;
  typedef typename Traits::pointer pointer;
  typedef typename Traits::reference reference;

 protected:
  IteratorBase()
  {
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:IteratorBase")
#endif
};

} /* namespace Freestyle */

#endif  // BASEITERATOR_H
