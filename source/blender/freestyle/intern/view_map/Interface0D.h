/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Interface to 0D elements.
 */

#include <iostream>
#include <string>

#include "../geometry/Geom.h"

#include "../system/Id.h"
#include "../system/Iterator.h"
#include "../system/Precision.h"

#include "../winged_edge/Nature.h"

#include "MEM_guardedalloc.h"

using namespace std;

namespace Freestyle {

//
// Interface0D
//
//////////////////////////////////////////////////

class FEdge;
class SVertex;
class ViewVertex;
class NonTVertex;
class TVertex;

/** Base class for any 0D element. */
class Interface0D {
 public:
  /** Default constructor */
  Interface0D() {}

  /** Destructor */
  virtual ~Interface0D() {};

  /** Returns the string "Interface0D". */
  virtual string getExactTypeName() const
  {
    return "Interface0D";
  }

  // Data access methods

  /** Returns the 3D x coordinate of the point. */
  virtual real getX() const;

  /** Returns the 3D y coordinate of the point. */
  virtual real getY() const;

  /** Returns the 3D z coordinate of the point. */
  virtual real getZ() const;

  /** Returns the 3D point. */
  virtual Geometry::Vec3r getPoint3D() const;

  /** Returns the 2D x coordinate of the point. */
  virtual real getProjectedX() const;

  /** Returns the 2D y coordinate of the point. */
  virtual real getProjectedY() const;

  /** Returns the 2D z coordinate of the point. */
  virtual real getProjectedZ() const;

  /** Returns the 2D point. */
  virtual Geometry::Vec2r getPoint2D() const;

  /** Returns the FEdge that lies between this Interface0D and the Interface0D given as argument.
   */
  virtual FEdge *getFEdge(Interface0D &);

  /** Returns the Id of the point. */
  virtual Id getId() const;

  /** Returns the nature of the point. */
  virtual Nature::VertexNature getNature() const;

  /** Cast the Interface0D in SVertex if it can be. */
  virtual SVertex *castToSVertex();

  /** Cast the Interface0D in ViewVertex if it can be. */
  virtual ViewVertex *castToViewVertex();

  /** Cast the Interface0D in NonTVertex if it can be. */
  virtual NonTVertex *castToNonTVertex();

  /** Cast the Interface0D in TVertex if it can be. */
  virtual TVertex *castToTVertex();

  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Interface0D")
};

//
// Interface0DIteratorNested
//
//////////////////////////////////////////////////

class Interface0DIteratorNested : public Iterator {
 public:
  virtual ~Interface0DIteratorNested() {}

  virtual string getExactTypeName() const
  {
    return "Interface0DIteratorNested";
  }

  virtual Interface0D &operator*() = 0;

  virtual Interface0D *operator->()
  {
    return &(operator*());
  }

  virtual int increment() = 0;

  virtual int decrement() = 0;

  virtual bool isBegin() const = 0;

  virtual bool isEnd() const = 0;

  virtual bool operator==(const Interface0DIteratorNested &it) const = 0;

  virtual bool operator!=(const Interface0DIteratorNested &it) const
  {
    return !(*this == it);
  }

  /** Returns the curvilinear abscissa */
  virtual float t() const = 0;

  /** Returns the point parameter 0<u<1 */
  virtual float u() const = 0;

  virtual Interface0DIteratorNested *copy() const = 0;
};

//
// Interface0DIterator
//
//////////////////////////////////////////////////

/** Class defining an iterator over Interface0D elements.
 *  An instance of this iterator is always obtained from a 1D element.
 * \attention In the scripting language, you must call \code it2 = Interface0DIterator(it1)
 * \endcode instead of \code it2 = it1 \endcode where \a it1 and \a it2 are 2 Interface0DIterator.
 *  Otherwise, incrementing \a it1 will also increment \a it2.
 */
class Interface0DIterator : public Iterator {
 public:
  Interface0DIterator(Interface0DIteratorNested *it = nullptr)
  {
    _iterator = it;
  }

  /** Copy constructor */
  Interface0DIterator(const Interface0DIterator &it)
  {
    _iterator = it._iterator->copy();
  }

  /** Destructor */
  virtual ~Interface0DIterator()
  {
    if (_iterator) {
      delete _iterator;
    }
  }

  /** Operator =
   *  \attention In the scripting language, you must call \code it2 = Interface0DIterator(it1)
   * \endcode instead of \code it2 = it1 \endcode where \a it1 and \a it2 are 2
   * Interface0DIterator. Otherwise, incrementing \a it1 will also increment \a it2.
   */
  Interface0DIterator &operator=(const Interface0DIterator &it)
  {
    if (_iterator) {
      delete _iterator;
    }
    _iterator = it._iterator->copy();
    return *this;
  }

  /** Returns the string "Interface0DIterator". */
  virtual string getExactTypeName() const
  {
    if (!_iterator) {
      return "Interface0DIterator";
    }
    return _iterator->getExactTypeName() + "Proxy";
  }

  // FIXME test it != 0 (exceptions ?)

  /** Returns a reference to the pointed Interface0D.
   *  In the scripting language, you must call "getObject()" instead using this operator.
   */
  Interface0D &operator*()
  {
    return _iterator->operator*();
  }

  /** Returns a pointer to the pointed Interface0D.
   *  Can't be called in the scripting language.
   */
  Interface0D *operator->()
  {
    return &(operator*());
  }

  /** Increments. In the scripting language, call "increment()". */
  Interface0DIterator &operator++()
  {
    _iterator->increment();
    return *this;
  }

  /** Increments. In the scripting language, call "increment()". */
  Interface0DIterator operator++(int)
  {
    Interface0DIterator ret(*this);
    _iterator->increment();
    return ret;
  }

  /** Decrements. In the scripting language, call "decrement()". */
  Interface0DIterator &operator--()
  {
    _iterator->decrement();
    return *this;
  }

  /** Decrements. In the scripting language, call "decrement()". */
  Interface0DIterator operator--(int)
  {
    Interface0DIterator ret(*this);
    _iterator->decrement();
    return ret;
  }

  /** Increments. */
  virtual int increment()
  {
    return _iterator->increment();
  }

  /** Decrements. */
  virtual int decrement()
  {
    return _iterator->decrement();
  }

  /** Returns true if the pointed Interface0D is the first of the 1D element containing the points
   * over which we're iterating.
   */
  virtual bool isBegin() const
  {
    return _iterator->isBegin();
  }

  /** Returns true if the pointed Interface0D is after the after the last point of the 1D element
   * we're iterating from. */
  virtual bool isEnd() const
  {
    return _iterator->isEnd();
  }

  /** Returns true when the iterator is pointing to the final valid element. */
  virtual bool atLast() const
  {
    if (_iterator->isEnd()) {
      return false;
    }

    _iterator->increment();
    bool result = _iterator->isEnd();
    _iterator->decrement();
    return result;
  }

  /** operator `==`. */
  bool operator==(const Interface0DIterator &it) const
  {
    return _iterator->operator==(*(it._iterator));
  }

  /** operator `!=`. */
  bool operator!=(const Interface0DIterator &it) const
  {
    return !(*this == it);
  }

  /** Returns the curvilinear abscissa. */
  inline float t() const
  {
    return _iterator->t();
  }

  /** Returns the point parameter in the curve 0<=u<=1. */
  inline float u() const
  {
    return _iterator->u();
  }

 protected:
  Interface0DIteratorNested *_iterator;
};

} /* namespace Freestyle */
