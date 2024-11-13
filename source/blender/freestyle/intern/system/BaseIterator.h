/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Classes defining the basic "Iterator" design pattern
 */

#include <iterator>

#include "MEM_guardedalloc.h"

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

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Const_traits")
};

template<class Element> class Nonconst_traits {
 public:
  typedef Element value_type;
  typedef Element &reference;
  typedef Element *pointer;
  typedef ptrdiff_t difference_type;
  typedef Nonconst_traits<Element> Non_const_traits;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Nonconst_traits")
};

class InputIteratorTag_Traits {
 public:
  typedef std::input_iterator_tag iterator_category;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:InputIteratorTag_Traits")
};

class BidirectionalIteratorTag_Traits {
 public:
  typedef std::bidirectional_iterator_tag iterator_category;

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BidirectionalIteratorTag_Traits")
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

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:IteratorBase")
};

} /* namespace Freestyle */
