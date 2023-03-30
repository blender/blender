/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

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
  virtual ~IteratorBase() {}

  virtual bool begin() const = 0;
  virtual bool end() const = 0;

  typedef typename IteratorTagTraits::iterator_category iterator_category;
  typedef typename Traits::value_type value_type;
  typedef typename Traits::difference_type difference_type;
  typedef typename Traits::pointer pointer;
  typedef typename Traits::reference reference;

 protected:
  IteratorBase() {}

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:IteratorBase")
#endif
};

} /* namespace Freestyle */
