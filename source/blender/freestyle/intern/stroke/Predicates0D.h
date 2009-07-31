//
//  Filename         : Predicates0D.h
//  Author(s)        : Stephane Grabli, Emmanuel Turquin
//  Purpose          : Class gathering stroke creation algorithms
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

#ifndef  PREDICATES0D_H
# define PREDICATES0D_H

# include "../view_map/Functions0D.h"

# include  "../python/Director.h"

//
// UnaryPredicate0D (base class for predicates in 0D)
//
///////////////////////////////////////////////////////////
/*! Base class for Unary Predicates that work
 *  on Interface0DIterator.
 *  A UnaryPredicate0D is a functor that evaluates
 *  a condition on a Interface0DIterator and returns
 *  true or false depending on whether this condition is
 *  satisfied or not.
 *  The UnaryPredicate0D is used by calling its () operator.
 *  Any inherited class must overload the () operator.
 */
class UnaryPredicate0D
{
public:
	
	bool result;
	PyObject *py_up0D;
	
  /*! Default constructor. */
	UnaryPredicate0D() { py_up0D = 0; }
  /*! Destructor. */
  virtual ~UnaryPredicate0D() {}
  /*! Returns the string of the name
   *  of the UnaryPredicate0D.
   */
  virtual string getName() const {
    return "UnaryPredicate0D";
  }
  /*! The () operator. Must be overload
   *  by inherited classes.
   *  \param it
   *    The Interface0DIterator pointing onto the
   *    Interface0D at which we wish to evaluate
   *    the predicate.
   *  \return true if the condition is satisfied,
   *    false otherwise.
   */
  virtual int operator()(Interface0DIterator& it) {
	return Director_BPy_UnaryPredicate0D___call__(this, it);
  }

};


//
// BinaryPredicate0D (base class for predicates in 0D)
//
///////////////////////////////////////////////////////////
/*! Base class for Binary Predicates working on Interface0D.
 *  A BinaryPredicate0D is typically an ordering relation
 *  between two Interface0D.
 *  It evaluates a relation between 2 Interface0D and
 *  returns true or false.
 *  It is used by calling the () operator.
 */
class BinaryPredicate0D
{
public:
	
		bool result;
		PyObject *py_bp0D;
	
  /*! Default constructor. */
		BinaryPredicate0D() { py_bp0D = 0; }
  /*! Destructor. */
  virtual ~BinaryPredicate0D() {}
  /*! Returns the string of the name of the
   * binary predicate.
   */
  virtual string getName() const {
    return "BinaryPredicate0D";
  }

  /*! The () operator. Must be overload by inherited classes.
   *  It evaluates a relation between 2 Interface0D.
   *  \param inter1
   *    The first Interface0D.
   *  \param inter2
   *    The second Interface0D.
   *  \return true or false.
   */
  virtual int operator()(Interface0D& inter1, Interface0D& inter2) {
	return Director_BPy_BinaryPredicate0D___call__(this, inter1, inter2);
  }

};


//
// Predicates definitions
//
///////////////////////////////////////////////////////////

namespace Predicates0D {

  // TrueUP0D
  /*! Returns true any time */
  class TrueUP0D : public UnaryPredicate0D
  {
  public:
    /*! Default constructor. */
    TrueUP0D() {}
    /*! Returns the string "TrueUP0D"*/
    string getName() const {
      return "TrueUP0D";
    }
    /*! The () operator. */
    int operator()(Interface0DIterator&) {
	  result = true;
	  return 0;
    }
  };

  // FalseUP0D
  /*! Returns false any time */
  class FalseUP0D : public UnaryPredicate0D
  {
  public:
    /*! Default constructor. */
    FalseUP0D() {}
    /*! Returns the string "FalseUP0D"*/
    string getName() const {
      return "FalseUP0D";
    }
    /*! The () operator. */
    int operator()(Interface0DIterator&) {
	  result = false;
      return 0;
    }
  };

} // end of namespace Predicates0D

#endif // PREDICATES0D_H
