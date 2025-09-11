/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Iterators used to iterate over the various elements of the ViewMap.
 *         These iterators can't be exported to python.
 */

#include "ViewMap.h"
#include "ViewMapIterators.h"

#include "../system/Iterator.h"  //soc

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*             ViewMap            */
/*                                */
/*                                */
/**********************************/

/**********************************/
/*                                */
/*                                */
/*             ViewVertex         */
/*                                */
/*                                */
/**********************************/

namespace ViewVertexInternal {

class edge_const_traits : public Const_traits<ViewVertex::directedViewEdge> {
 public:
  typedef vector<ViewVertex::directedViewEdge> edges_container;
  typedef edges_container::const_iterator edges_container_iterator;
  typedef vector<ViewVertex::directedViewEdge *> edge_pointers_container;
  typedef edge_pointers_container::const_iterator edge_pointers_container_iterator;
};

class edge_nonconst_traits : public Nonconst_traits<ViewVertex::directedViewEdge> {
 public:
  typedef vector<ViewVertex::directedViewEdge> edges_container;
  typedef edges_container::iterator edges_container_iterator;
  typedef vector<ViewVertex::directedViewEdge *> edge_pointers_container;
  typedef edge_pointers_container::iterator edge_pointers_container_iterator;
};

template<class Traits>
class edge_iterator_base : public IteratorBase<Traits, InputIteratorTag_Traits> {
 public:
  typedef typename Traits::value_type value_type;
  typedef typename Traits::difference_type difference_type;
  typedef typename Traits::pointer pointer;
  typedef typename Traits::reference reference;
  typedef edge_iterator_base<Traits> Self;
  typedef typename Traits::edges_container_iterator edges_container_iterator;
  typedef typename Traits::edge_pointers_container_iterator edge_pointers_container_iterator;
  typedef edge_iterator_base<edge_nonconst_traits> iterator;
  typedef edge_iterator_base<edge_const_traits> const_iterator;

 public:
  friend class ViewVertex;
  friend class TVertex;
  friend class NonTVertex;
  friend class ViewEdge;
  friend class edge_iterator;

 protected:
  Nature::VertexNature _Nature;  // the nature of the underlying vertex
  // T vertex attributes
  edge_pointers_container_iterator _tbegin;
  edge_pointers_container_iterator _tend;
  edge_pointers_container_iterator _tvertex_iter;

#if 0
  mutable value_type _tvertex_iter;
  value_type _feA;
  value_type _feB;
  value_type _beA;
  value_type _beB;
#endif

  // Non TVertex attributes
  edges_container_iterator _begin;
  edges_container_iterator _end;
  edges_container_iterator _nontvertex_iter;

  typedef IteratorBase<Traits, InputIteratorTag_Traits> parent_class;

 public:
  inline edge_iterator_base() : parent_class() {}

  inline edge_iterator_base(Nature::VertexNature iNature) : parent_class()
  {
    _Nature = iNature;
  }

  edge_iterator_base(const edge_iterator_base<edge_nonconst_traits> &iBrother)
      : parent_class(iBrother)
  {
    _Nature = iBrother._Nature;
    if (_Nature & Nature::T_VERTEX) {
#if 0
      _feA = iBrother._feA;
      _feB = iBrother._feB;
      _beA = iBrother._beA;
      _beB = iBrother._beB;
      _tvertex_iter = iBrother._tvertex_iter;
#endif
      _tbegin = iBrother._tbegin;
      _tend = iBrother._tend;
      _tvertex_iter = iBrother._tvertex_iter;
    }
    else {
      _begin = iBrother._begin;
      _end = iBrother._end;
      _nontvertex_iter = iBrother._nontvertex_iter;
    }
  }

  edge_iterator_base(const edge_iterator_base<edge_const_traits> &iBrother)
      : parent_class(iBrother)
  {
    _Nature = iBrother._Nature;
    if (_Nature & Nature::T_VERTEX) {
#if 0
      _feA = iBrother._feA;
      _feB = iBrother._feB;
      _beA = iBrother._beA;
      _beB = iBrother._beB;
      _tvertex_iter = iBrother._tvertex_iter;
#endif
      _tbegin = iBrother._tbegin;
      _tend = iBrother._tend;
      _tvertex_iter = iBrother._tvertex_iter;
    }
    else {
      _begin = iBrother._begin;
      _end = iBrother._end;
      _nontvertex_iter = iBrother._nontvertex_iter;
    }
  }

  virtual ~edge_iterator_base() {}

  // protected://FIXME
 public:
#if 0
  inline edge_iterator_base(
      value_type ifeA, value_type ifeB, value_type ibeA, value_type ibeB, value_type iter)
      : parent_class()
  {
    _Nature = Nature::T_VERTEX;
    _feA = ifeA;
    _feB = ifeB;
    _beA = ibeA;
    _beB = ibeB;
    _tvertex_iter = iter;
  }
#endif

  inline edge_iterator_base(edge_pointers_container_iterator begin,
                            edge_pointers_container_iterator end,
                            edge_pointers_container_iterator iter)
      : parent_class()
  {
    _Nature = Nature::T_VERTEX;
    _tbegin = begin;
    _tend = end;
    _tvertex_iter = iter;
  }

  inline edge_iterator_base(edges_container_iterator begin,
                            edges_container_iterator end,
                            edges_container_iterator iter)
      : parent_class()
  {
    _Nature = Nature::NON_T_VERTEX;
    _begin = begin;
    _end = end;
    _nontvertex_iter = iter;
  }

 public:
  virtual bool begin() const
  {
    if (_Nature & Nature::T_VERTEX) {
      return (_tvertex_iter == _tbegin);
      // return (_tvertex_iter == _feA);
    }
    else {
      return (_nontvertex_iter == _begin);
    }
  }

  virtual bool end() const
  {
    if (_Nature & Nature::T_VERTEX) {
      // return (_tvertex_iter.first == 0);
      return (_tvertex_iter == _tend);
    }
    else {
      return (_nontvertex_iter == _end);
    }
  }

  // operators
  // operator corresponding to ++i
  virtual Self &operator++()
  {
    increment();
    return *this;
  }

  // operator corresponding to i++, i.e. which returns the value *and then* increments it.
  // That's why we store the value in a temp.
  virtual Self operator++(int)
  {
    Self tmp = *this;
    increment();
    return tmp;
  }

  // comparability
  virtual bool operator!=(const Self &b) const
  {
    if (_Nature & Nature::T_VERTEX) {
      return (_tvertex_iter != b._tvertex_iter);
    }
    else {
      return (_nontvertex_iter != b._nontvertex_iter);
    }
  }

  virtual bool operator==(const Self &b) const
  {
    return !(*this != b);
  }

  // dereferencing
  virtual reference operator*() const
  {
    if (_Nature & Nature::T_VERTEX) {
      // return _tvertex_iter;
      return **_tvertex_iter;
    }
    else {
      return (*_nontvertex_iter);
    }
  }

  virtual pointer operator->() const
  {
    return &(operator*());
  }

 protected:
  inline void increment()
  {
    if (_Nature & Nature::T_VERTEX) {
      value_type tmp = (**_tvertex_iter);
      ++_tvertex_iter;
      value_type tmp2 = (**_tvertex_iter);
      if (tmp2.first == tmp.first) {
        ++_tvertex_iter;
      }
#if 0
      // Hack to deal with cusp. the result of a cusp is a TVertex having two identical viewedges.
      // In order to iterate properly, we chose to skip these last ones.
      if (_feB.first == _beA.first) {
        if (_feA.first == _beB.first) {
          _tvertex_iter.first = 0;
          return;
        }

        if (_tvertex_iter.first == _feA.first) {
          _tvertex_iter.first = _beB.first;
        }
        else if (_tvertex_iter.first == _beB.first) {
          _tvertex_iter.first = 0;
        }
        else {
          _tvertex_iter.first = _feA.first;
        }
        return;
      }
      if (_feA.first == _beB.first) {
        if (_feB.first == _beA.first) {
          _tvertex_iter.first = 0;
          return;
        }

        if (_tvertex_iter.first == _feB.first) {
          _tvertex_iter.first = _beA.first;
        }
        else if (_tvertex_iter.first == _beA.first) {
          _tvertex_iter.first = 0;
        }
        else {
          _tvertex_iter.first = _feB.first;
        }
        return;
      }
      // End of hack

      if (_tvertex_iter.first == _feA.first) {
        // we return bea or beb
        // choose one of them
        _tvertex_iter.first = _feB.first;
        return;
      }
      if (_tvertex_iter.first == _feB.first) {
        _tvertex_iter.first = _beA.first;
        return;
      }
      if (_tvertex_iter.first == _beA.first) {
        _tvertex_iter.first = _beB.first;
        return;
      }
      if (_tvertex_iter.first == _beB.first) {
        _tvertex_iter.first = 0;
        return;
      }
#endif
    }
    else {
      ++_nontvertex_iter;
    }
  }
};

}  // namespace ViewVertexInternal

/**********************************/
/*                                */
/*                                */
/*             ViewEdge           */
/*                                */
/*                                */
/**********************************/

namespace ViewEdgeInternal {

/**----------------------*/
/** Iterators definition */
/**----------------------*/
template<class Traits>
class edge_iterator_base : public IteratorBase<Traits, BidirectionalIteratorTag_Traits> {
 public:
  typedef typename Traits::value_type value_type;
  typedef typename Traits::difference_type difference_type;
  typedef typename Traits::pointer pointer;
  typedef typename Traits::reference reference;
  typedef edge_iterator_base<Traits> Self;

 public:
  mutable value_type _ViewEdge;
  // friend class edge_iterator_base<Nonconst_traits<ViewEdge*> >;
  // friend class edge_iterator_base<Const_traits<ViewEdge*> >;
  value_type _first;
  bool _orientation;
  typedef IteratorBase<Traits, BidirectionalIteratorTag_Traits> parent_class;

 public:
  friend class ViewEdge;
  inline edge_iterator_base() : parent_class()
  {
    _orientation = true;
    _first = 0;
  }

  inline edge_iterator_base(const edge_iterator_base<Nonconst_traits<ViewEdge *>> &iBrother)
      : parent_class()
  {
    _ViewEdge = iBrother._ViewEdge;
    _first = iBrother._first;
    _orientation = iBrother._orientation;
  }

  inline edge_iterator_base(const edge_iterator_base<Const_traits<ViewEdge *>> &iBrother)
      : parent_class()
  {
    _ViewEdge = iBrother._ViewEdge;
    _first = iBrother._first;
    _orientation = iBrother._orientation;
  }

  // protected://FIXME
 public:
  inline edge_iterator_base(value_type iEdge, bool orientation = true) : parent_class()
  {
    _ViewEdge = iEdge;
    _first = iEdge;
    _orientation = orientation;
  }

 public:
  virtual Self *clone() const
  {
    return new edge_iterator_base(*this);
  }

  virtual ~edge_iterator_base() {}

 public:
  virtual bool orientation()
  {
    return _orientation;
  }

  virtual void set_edge(value_type iVE)
  {
    _ViewEdge = iVE;
  }

  virtual void set_orientation(bool iOrientation)
  {
    _orientation = iOrientation;
  }

  virtual void change_orientation()
  {
    _orientation = !_orientation;
  }

  // operators
  // operator corresponding to ++i
  inline Self &operator++()
  {
    //++_ViewEdge->getTimeStamp();
    increment();
    return *this;
  }

  // operator corresponding to i++, i.e. which returns the value *and then* increments it.
  // That's why we store the value in a temp.
  inline Self operator++(int)
  {
    //++_ViewEdge->getTimeStamp();
    Self tmp = *this;
    increment();
    return tmp;
  }

  // operator corresponding to --i
  inline Self &operator--()
  {
    //++_ViewEdge->getTimeStamp();
    decrement();
    return *this;
  }

  // operator corresponding to i--, i.e. which returns the value *and then* increments it.
  // That's why we store the value in a temp.
  inline Self operator--(int)
  {
    //++_ViewEdge->getTimeStamp();
    Self tmp = *this;
    decrement();
    return tmp;
  }

  // comparability
  virtual bool operator!=(const Self &b) const
  {
    return (_ViewEdge != b._ViewEdge);
  }

  virtual bool operator==(const Self &b) const
  {
    return !(*this != b);
  }

  // dereferencing
  virtual reference operator*() const
  {
    return _ViewEdge;
  }

  virtual pointer operator->() const
  {
    return &(operator*());
  }

 public:
  virtual bool begin() const
  {
    return (_ViewEdge == _first) ? true : false;
  }

  virtual bool end() const
  {
    return (_ViewEdge == 0) ? true : false;
  }

 protected:
  virtual void increment() {}
  virtual void decrement() {}
};

template<class Traits>
class fedge_iterator_base : public IteratorBase<Traits, BidirectionalIteratorTag_Traits> {
 public:
  typedef typename Traits::value_type value_type;
  typedef typename Traits::difference_type difference_type;
  typedef typename Traits::pointer pointer;
  typedef typename Traits::reference reference;
  typedef fedge_iterator_base<Traits> Self;

 public:
  typedef IteratorBase<Traits, BidirectionalIteratorTag_Traits> parent_class;
  mutable value_type _FEdge;
  value_type _first;
  value_type _FEdgeB;  // last fedge of the view edge

 public:
  friend class ViewEdge;
  friend class fedge_iterator;

  inline fedge_iterator_base() : parent_class() {}

  inline fedge_iterator_base(const fedge_iterator_base<Nonconst_traits<FEdge *>> &iBrother)
      : parent_class()
  {
    _FEdge = iBrother._FEdge;
    _first = iBrother._first;
    _FEdgeB = iBrother._FEdgeB;
  }

  inline fedge_iterator_base(const fedge_iterator_base<Const_traits<FEdge *>> &iBrother)
      : parent_class()
  {
    _FEdge = iBrother._FEdge;
    _first = iBrother._first;
    _FEdgeB = iBrother._FEdgeB;
  }

  // protected://FIXME
 public:
  inline fedge_iterator_base(value_type iEdge, value_type iFEdgeB) : parent_class()
  {
    _FEdge = iEdge;
    _first = iEdge;
    _FEdgeB = iFEdgeB;
  }

 public:
  virtual ~fedge_iterator_base() {}

  // operators
  // operator corresponding to ++i.
  inline Self &operator++()
  {
    increment();
    return *this;
  }

  // operator corresponding to i++, i.e. which returns the value *and then* increments it.
  // That's why we store the value in a temp.
  inline Self operator++(int)
  {
    Self tmp = *this;
    increment();
    return tmp;
  }

  // operator corresponding to --i
  inline Self &operator--()
  {
    decrement();
    return *this;
  }

  // operator corresponding to i--, i.e. which returns the value *and then* increments it.
  // That's why we store the value in a temp.
  inline Self operator--(int)
  {
    Self tmp = *this;
    decrement();
    return tmp;
  }

  // comparability
  virtual bool operator!=(const Self &b) const
  {
    return (_FEdge != b._FEdge);
  }

  virtual bool operator==(const Self &b) const
  {
    return !(*this != b);
  }

  // dereferencing
  virtual reference operator*() const
  {
    return _FEdge;
  }

  virtual pointer operator->() const
  {
    return &(operator*());
  }

 public:
  virtual bool begin() const
  {
    return (_FEdge == _first) ? true : false;
  }

  virtual bool end() const
  {
    return (_FEdge == 0) ? true : false;
  }

 protected:
  virtual void increment()
  {
    _FEdge = _FEdge->nextEdge();  // we don't change or
  }

  virtual void decrement()
  {
    if (0 == _FEdge) {
      _FEdge = _FEdgeB;
      return;
    }
    _FEdge = _FEdge->previousEdge();  // we don't change or
  }
};

template<class Traits>
class vertex_iterator_base : public IteratorBase<Traits, BidirectionalIteratorTag_Traits> {
 public:
  typedef typename Traits::value_type value_type;
  typedef typename Traits::difference_type difference_type;
  typedef typename Traits::pointer pointer;
  typedef typename Traits::reference reference;
  typedef vertex_iterator_base<Traits> Self;

 protected:
  typedef IteratorBase<Traits, BidirectionalIteratorTag_Traits> parent_class;

 public:
  mutable value_type _SVertex;
  FEdge *_NextFEdge;
  FEdge *_PreviousFEdge;

 public:
  friend class ViewEdge;
  friend class vertex_iterator;

  inline vertex_iterator_base() : parent_class() {}

  inline vertex_iterator_base(const vertex_iterator_base<Const_traits<SVertex *>> &iBrother)
      : parent_class()
  {
    _SVertex = iBrother._SVertex;
    _NextFEdge = iBrother._NextFEdge;
    _PreviousFEdge = iBrother._PreviousFEdge;
  }

  inline vertex_iterator_base(const vertex_iterator_base<Nonconst_traits<SVertex *>> &iBrother)
      : parent_class()
  {
    _SVertex = iBrother._SVertex;
    _NextFEdge = iBrother._NextFEdge;
    _PreviousFEdge = iBrother._PreviousFEdge;
  }

  // protected://FIXME
 public:
  inline vertex_iterator_base(value_type iVertex, FEdge *iPreviousFEdge, FEdge *iNextFEdge)
      : parent_class()
  {
    _SVertex = iVertex;
    _NextFEdge = iNextFEdge;
    _PreviousFEdge = iPreviousFEdge;
  }

 public:
  virtual ~vertex_iterator_base() {}

  virtual bool begin() const
  {
    return (_PreviousFEdge == 0) ? true : false;
  }

  virtual bool end() const
  {
    return (_SVertex == 0) ? true : false;
  }

  // operators
  // operator corresponding to ++i
  inline Self &operator++()
  {
    increment();
    return *this;
  }

  // operator corresponding to i++, i.e. which returns the value *and then* increments it.
  // That's why we store the value in a temp.
  inline Self operator++(int)
  {
    Self tmp = *this;
    increment();
    return tmp;
  }

  // operator corresponding to --i
  inline Self &operator--()
  {
    decrement();
    return *this;
  }

  // operator corresponding to --i, i.e. which returns the value *and then* increments it.
  // That's why we store the value in a temp.
  inline Self operator--(int)
  {
    Self tmp = *this;
    decrement();
    return tmp;
  }

  // comparability
  virtual bool operator!=(const Self &b) const
  {
    return (_SVertex != b._SVertex);
  }

  virtual bool operator==(const Self &b) const
  {
    return !(*this != b);
  }

  // dereferencing
  virtual reference operator*() const
  {
    return _SVertex;
  }

  virtual pointer operator->() const
  {
    return &(operator*());
  }

 protected:
  virtual void increment()
  {
    if (!_NextFEdge) {
      _SVertex = nullptr;
      return;
    }
    _SVertex = _NextFEdge->vertexB();
    _PreviousFEdge = _NextFEdge;
    _NextFEdge = _NextFEdge->nextEdge();
  }

  virtual void decrement()
  {
#if 0
    if (!_SVertex) {
      _SVertex = _PreviousFEdge->vertexB();
      return;
    }
#endif
    if (!_PreviousFEdge) {
      _SVertex = nullptr;
      return;
    }
    _SVertex = _PreviousFEdge->vertexA();
    _NextFEdge = _PreviousFEdge;
    _PreviousFEdge = _PreviousFEdge->previousEdge();
  }
};

}  // end of namespace ViewEdgeInternal

} /* namespace Freestyle */
