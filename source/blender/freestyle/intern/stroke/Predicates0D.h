/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_PREDICATES_0D_H__
#define __FREESTYLE_PREDICATES_0D_H__

/** \file blender/freestyle/intern/stroke/Predicates0D.h
 *  \ingroup freestyle
 *  \brief Class gathering stroke creation algorithms
 *  \author Stephane Grabli
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#include "../python/Director.h"

#include "../view_map/Functions0D.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

//
// UnaryPredicate0D (base class for predicates in 0D)
//
///////////////////////////////////////////////////////////

/*! Base class for Unary Predicates that work on Interface0DIterator.
 *  A UnaryPredicate0D is a functor that evaluates a condition on a Interface0DIterator and returns
 *  true or false depending on whether this condition is satisfied or not.
 *  The UnaryPredicate0D is used by calling its () operator.
 *  Any inherited class must overload the () operator.
 */
class UnaryPredicate0D
{
public:
	bool result;
	PyObject *py_up0D;

	/*! Default constructor. */
	UnaryPredicate0D()
	{
		py_up0D = 0;
	}

	/*! Destructor. */
	virtual ~UnaryPredicate0D() {}

	/*! Returns the string of the name of the UnaryPredicate0D. */
	virtual string getName() const
	{
		return "UnaryPredicate0D";
	}

	/*! The () operator. Must be overload by inherited classes.
	 *  \param it
	 *    The Interface0DIterator pointing onto the Interface0D at which we wish to evaluate the predicate.
	 *  \return true if the condition is satisfied, false otherwise.
	 */
	virtual int operator()(Interface0DIterator& it)
	{
		return Director_BPy_UnaryPredicate0D___call__(this, it);
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:UnaryPredicate0D")
#endif
};


//
// BinaryPredicate0D (base class for predicates in 0D)
//
///////////////////////////////////////////////////////////

/*! Base class for Binary Predicates working on Interface0D.
 *  A BinaryPredicate0D is typically an ordering relation between two Interface0D.
 *  It evaluates a relation between 2 Interface0D and returns true or false.
 *  It is used by calling the () operator.
 */
class BinaryPredicate0D
{
public:
	bool result;
	PyObject *py_bp0D;

	/*! Default constructor. */
	BinaryPredicate0D()
	{
		py_bp0D = 0;
	}

	/*! Destructor. */
	virtual ~BinaryPredicate0D() {}

	/*! Returns the string of the name of the binary predicate. */
	virtual string getName() const
	{
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
	virtual int operator()(Interface0D& inter1, Interface0D& inter2)
	{
		return Director_BPy_BinaryPredicate0D___call__(this, inter1, inter2);
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BinaryPredicate0D")
#endif
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
	string getName() const
	{
		return "TrueUP0D";
	}

	/*! The () operator. */
	int operator()(Interface0DIterator&)
	{
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
	string getName() const
	{
		return "FalseUP0D";
	}

	/*! The () operator. */
	int operator()(Interface0DIterator&)
	{
		result = false;
		return 0;
	}
};

} // end of namespace Predicates0D

} /* namespace Freestyle */

#endif // __FREESTYLE_PREDICATES_0D_H__
