//
//  Filename         : Id.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Identification system
//  Date of creation : 01/07/2003
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef  ID_H
# define ID_H

/*! Class used to tag any object by an id .
 *  It is made of two unsigned integers.
 */
class Id
{
public:

  typedef unsigned id_type;

  /*! Default constructor */
  Id() {
    _first = 0;
    _second = 0;
  }

  /*! Builds an Id from an integer.
   *  The second number is set to 0.
   */
  Id(id_type id) {
    _first = id;
    _second = 0;
  }

  /*! Builds the Id from the two numbers */
  Id(id_type ifirst, id_type isecond) {
    _first = ifirst;
    _second = isecond;
  }

  /*! Copy constructor */
  Id(const Id& iBrother) {
    _first = iBrother._first;
    _second = iBrother._second;
  }

  /*! Operator= */
  Id& operator=(const Id& iBrother) {
   _first = iBrother._first;
   _second = iBrother._second;
    return *this;
  } 

  /*! Returns the first Id number */
  id_type getFirst() const {
    return _first;
  }

  /*! Returns the second Id number */
  id_type getSecond() const {
    return _second;
  }

  /*! Sets the first number constituing the Id */
  void setFirst(id_type first) {
    _first = first;
  }

  /*! Sets the second number constituing the Id */
  void setSecond(id_type second) {
    _second = second;
  }
  
  /*! Operator== */
  bool operator==(const Id& id) const {
    return ((_first == id._first) && (_second == id._second));
  }

  /*! Operator!= */
  bool operator!=(const Id& id) const {
    return !((*this)==id);
  }
  
  /*! Operator< */
  bool operator<(const Id& id) const {
    if (_first < id._first)
      return true;
    if (_first == id._first && _second < id._second)
      return true;
    return false;
}

private:

  id_type _first;
  id_type _second;
};

// stream operator
inline std::ostream& operator<<(std::ostream& s, const Id& id) {
  s << "[" << id.getFirst() << ", " << id.getSecond() << "]";
  return s;
}

# endif // ID_H
