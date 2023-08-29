/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Identification system
 */

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

/** Class used to tag any object by an id.
 *  It is made of two unsigned-integers.
 */
class Id {
 public:
  typedef uint id_type;

  /** Default constructor */
  Id()
  {
    _first = 0;
    _second = 0;
  }

  /** Builds an Id from an integer.
   *  The second number is set to 0.
   */
  Id(id_type id)
  {
    _first = id;
    _second = 0;
  }

  /** Builds the Id from the two numbers */
  Id(id_type ifirst, id_type isecond)
  {
    _first = ifirst;
    _second = isecond;
  }

  /** Copy constructor */
  Id(const Id &iBrother)
  {
    _first = iBrother._first;
    _second = iBrother._second;
  }

  /** Operator= */
  Id &operator=(const Id &iBrother)
  {
    _first = iBrother._first;
    _second = iBrother._second;
    return *this;
  }

  /** Returns the first Id number */
  id_type getFirst() const
  {
    return _first;
  }

  /** Returns the second Id number */
  id_type getSecond() const
  {
    return _second;
  }

  /** Sets the first number constituting the Id */
  void setFirst(id_type first)
  {
    _first = first;
  }

  /** Sets the second number constituting the Id */
  void setSecond(id_type second)
  {
    _second = second;
  }

  /** Operator== */
  bool operator==(const Id &id) const
  {
    return ((_first == id._first) && (_second == id._second));
  }

  /** Operator!= */
  bool operator!=(const Id &id) const
  {
    return !((*this) == id);
  }

  /** Operator< */
  bool operator<(const Id &id) const
  {
    if (_first < id._first) {
      return true;
    }
    if (_first == id._first && _second < id._second) {
      return true;
    }
    return false;
  }

 private:
  id_type _first;
  id_type _second;

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Id")
#endif
};

// stream operator
inline std::ostream &operator<<(std::ostream &s, const Id &id)
{
  s << "[" << id.getFirst() << ", " << id.getSecond() << "]";
  return s;
}

} /* namespace Freestyle */
