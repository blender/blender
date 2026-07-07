/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Iterators used to iterate over the elements of the Stroke. Can't be used in python
 */

#include "Stroke.h"
#include "StrokeIterators.h"

namespace Freestyle {

namespace StrokeInternal {

class vertex_const_traits : public Const_traits<StrokeVertex *> {
 public:
  typedef std::deque<StrokeVertex *> vertex_container;
  typedef vertex_container::const_iterator vertex_container_iterator;
};

class vertex_nonconst_traits : public Nonconst_traits<StrokeVertex *> {
 public:
  typedef std::deque<StrokeVertex *> vertex_container;  //! the vertices container
  typedef vertex_container::iterator vertex_container_iterator;
};

template<class Traits>
class vertex_iterator_base : public IteratorBase<Traits, BidirectionalIteratorTag_Traits> {
 public:
  typedef vertex_iterator_base<Traits> Self;

 protected:
  typedef IteratorBase<Traits, BidirectionalIteratorTag_Traits> parent_class;
  typedef typename Traits::vertex_container_iterator vertex_container_iterator;
  typedef vertex_iterator_base<vertex_nonconst_traits> iterator;
  typedef vertex_iterator_base<vertex_const_traits> const_iterator;

  // protected:
 public:
  vertex_container_iterator _it;
  vertex_container_iterator _begin;
  vertex_container_iterator _end;

 public:
  friend class Stroke;
  // friend class vertex_iterator;

  inline vertex_iterator_base() : parent_class() {}

  inline vertex_iterator_base(const iterator &iBrother) : parent_class()
  {
    _it = iBrother._it;
    _begin = iBrother._begin;
    _end = iBrother._end;
  }

  inline vertex_iterator_base(const const_iterator &iBrother) : parent_class()
  {
    _it = iBrother._it;
    _begin = iBrother._begin;
    _end = iBrother._end;
  }

  // protected: //FIXME
 public:
  inline vertex_iterator_base(vertex_container_iterator it,
                              vertex_container_iterator begin,
                              vertex_container_iterator end)
      : parent_class()
  {
    _it = it;
    _begin = begin;
    _end = end;
  }

 public:
  virtual ~vertex_iterator_base() {}

  virtual bool begin() const
  {
    return (_it == _begin) ? true : false;
  }

  virtual bool end() const
  {
    return (_it == _end) ? true : false;
  }

  // operators
  inline Self &operator++()  // operator corresponding to ++i
  {
    ++_it;
    return *(this);
  }

  /* Operator corresponding to i++, i.e. which returns the value *and then* increments.
   * That's why we store the value in a temp.
   */
  inline Self operator++(int)
  {
    Self tmp = *this;
    ++_it;
    return tmp;
  }

  inline Self &operator--()  // operator corresponding to --i
  {
    --_it;
    return *(this);
  }

  inline Self operator--(int)
  {
    Self tmp = *this;
    --_it;
    return tmp;
  }

  // comparability
  virtual bool operator!=(const Self &b) const
  {
    return (_it != b._it);
  }

  virtual bool operator==(const Self &b) const
  {
    return !(*this != b);
  }

  // dereferencing
  virtual typename Traits::reference operator*() const
  {
    return *(_it);
  }

  virtual typename Traits::pointer operator->() const
  {
    return &(operator*());
  }

  /** accessors */
  inline vertex_container_iterator it() const
  {
    return _it;
  }

  inline vertex_container_iterator getBegin() const
  {
    return _begin;
  }

  inline vertex_container_iterator getEnd() const
  {
    return _end;
  }
};

}  // end of namespace StrokeInternal

} /* namespace Freestyle */
