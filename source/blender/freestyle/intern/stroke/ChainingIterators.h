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

#ifndef __FREESTYLE_CHAINING_ITERATORS_H__
#define __FREESTYLE_CHAINING_ITERATORS_H__

/** \file blender/freestyle/intern/stroke/ChainingIterators.h
 *  \ingroup freestyle
 *  \brief Chaining iterators
 *  \author Stephane Grabli
 *  \date 01/07/2003
 */

#include <iostream>

#include "Predicates1D.h"

#include  "../python/Director.h"

#include "../system/Iterator.h" //soc 

#include "../view_map/ViewMap.h"
#include "../view_map/ViewMapIterators.h"
#include "../view_map/ViewMapAdvancedIterators.h"

//using namespace ViewEdgeInternal;

namespace Freestyle {

//
// Adjacency iterator used in the chaining process
//
///////////////////////////////////////////////////////////
class LIB_STROKE_EXPORT AdjacencyIterator : public Iterator
{
protected:
	ViewVertexInternal::orientedViewEdgeIterator _internalIterator;
	bool _restrictToSelection;
	bool _restrictToUnvisited;

public:
	AdjacencyIterator()
	{
		_restrictToSelection = true;
		_restrictToUnvisited = true;
	}

	AdjacencyIterator(ViewVertex *iVertex, bool iRestrictToSelection = true, bool iRestrictToUnvisited = true)
	{
		_restrictToSelection = iRestrictToSelection;
		_restrictToUnvisited = iRestrictToUnvisited;
		_internalIterator = iVertex->edgesBegin();
		while ((!_internalIterator.isEnd()) && (!isValid((*_internalIterator).first)))
			++_internalIterator;
	}

	AdjacencyIterator(const AdjacencyIterator& iBrother)
	{
		_internalIterator = iBrother._internalIterator;
		_restrictToSelection = iBrother._restrictToSelection;
		_restrictToUnvisited = iBrother._restrictToUnvisited;
	}

	AdjacencyIterator& operator=(const AdjacencyIterator& iBrother)
	{
		_internalIterator = iBrother._internalIterator;
		_restrictToSelection = iBrother._restrictToSelection;
		_restrictToUnvisited = iBrother._restrictToUnvisited;  
		return *this;
	}

	virtual ~AdjacencyIterator() {}

	virtual string getExactTypeName() const
	{
		return "AdjacencyIterator";
	}

	virtual inline bool isEnd() const
	{
		return _internalIterator.isEnd();
	}

	virtual inline bool isBegin() const
	{
		return _internalIterator.isBegin();
	}

	/*! Returns true if the current ViewEdge is is coming towards the iteration vertex. False otherwise. */
	bool isIncoming() const;

	/*! Returns a *pointer* to the pointed ViewEdge. */
	virtual ViewEdge *operator*();

	virtual ViewEdge *operator->()
	{
		return operator*();
	}

	virtual AdjacencyIterator& operator++()
	{
		increment();
		return *this;
	}

	virtual AdjacencyIterator operator++(int)
	{
		AdjacencyIterator tmp(*this);
		increment();
		return tmp;
	}

	virtual int increment();

	virtual int decrement()
	{
		cerr << "Warning: method decrement() not implemented" << endl;
		return 0;
	}

protected:
	bool isValid(ViewEdge *edge);
};

//
// Base class for Chaining Iterators
//
///////////////////////////////////////////////////////////

/*! Base class for chaining iterators.
 *  This class is designed to be overloaded in order to describe chaining rules.
 *  It makes the works of chaining rules description easier.
 *  The two main methods that need to overloaded are traverse() and init().
 *  traverse() tells which ViewEdge to follow, among the adjacent ones.
 *  If you specify restriction rules (such as "Chain only ViewEdges of the selection"), they will be included
 *  in the adjacency iterator. (i.e, the adjacent iterator will only stop on "valid" edges).
 */
class LIB_STROKE_EXPORT ChainingIterator : public ViewEdgeInternal::ViewEdgeIterator
{
protected:
	bool _restrictToSelection;
	bool _restrictToUnvisited;
	bool _increment; //true if we're currently incrementing, false when decrementing

public:
	ViewEdge *result;
	PyObject *py_c_it;

	/*! Builds a Chaining Iterator from the first ViewEdge used for iteration and its orientation.
	 *  \param iRestrictToSelection
	 *    Indicates whether to force the chaining to stay within the set of selected ViewEdges or not.
	 *  \param iRestrictToUnvisited
	 *    Indicates whether a ViewEdge that has already been chained must be ignored ot not.
	 *  \param begin
	 *    The ViewEdge from which to start the chain.
	 *  \param orientation
	 *    The direction to follow to explore the graph. If true, the direction indicated by the first ViewEdge is used.
	 */
	ChainingIterator(bool iRestrictToSelection = true, bool iRestrictToUnvisited = true, ViewEdge *begin = NULL,
	                 bool orientation = true)
	: ViewEdgeIterator(begin, orientation)
	{
		_restrictToSelection = iRestrictToSelection;
		_restrictToUnvisited = iRestrictToUnvisited;
		_increment = true;
		py_c_it = NULL;
	}

	/*! Copy constructor */
	ChainingIterator(const ChainingIterator& brother)
	: ViewEdgeIterator(brother)
	{
		_restrictToSelection = brother._restrictToSelection;
		_restrictToUnvisited = brother._restrictToUnvisited;
		_increment = brother._increment;
		py_c_it = brother.py_c_it;
	}

	/*! Returns the string "ChainingIterator" */
	virtual string getExactTypeName() const
	{
		return "ChainingIterator";
	}

	/*! Inits the iterator context.
	 *  This method is called each time a new chain is started.
	 *  It can be used to reset some history information that you might want to keep.
	 */
	virtual int init()
	{
		return Director_BPy_ChainingIterator_init(this);
	}

	/*! This method iterates over the potential next ViewEdges and returns the one that will be followed next.
	 *  returns the next ViewEdge to follow or 0 when the end of the chain is reached.
	 *  \param it
	 *    The iterator over the ViewEdges adjacent to the end vertex of the current ViewEdge.
	 *    The Adjacency iterator reflects the restriction rules by only iterating over the valid ViewEdges.
	 */
	virtual int traverse(const AdjacencyIterator &it)
	{
		return Director_BPy_ChainingIterator_traverse(this, const_cast<AdjacencyIterator &>(it));
	}

	/* accessors */
	/*! Returns true if the orientation of the current ViewEdge corresponds to its natural orientation */
	//inline bool getOrientation() const {}

	/*! Returns the vertex which is the next crossing */
	inline ViewVertex *getVertex()
	{
		if (_increment) {
			if (_orientation) {
				return _edge->B();
			}
			else {
				return _edge->A();
			}
		}
		else {
			if (_orientation) {
				return _edge->A();
			}
			else {
				return _edge->B();
			}
		}
	}

	/*! Returns true if the current iteration is an incrementation */
	inline bool isIncrementing() const
	{
		return _increment;
	}

	/* increments.*/
	virtual int increment();
	virtual int decrement();
};

//
// Chaining iterators definitions
//
///////////////////////////////////////////////////////////

/*! A ViewEdge Iterator used to follow ViewEdges the most naturally.
 *  For example, it will follow visible ViewEdges of same nature.
 *  As soon, as the nature or the visibility changes, the iteration stops (by setting the pointed ViewEdge to 0).
 *  In the case of an iteration over a set of ViewEdge that are both Silhouette and Crease, there will be a precedence
 *  of the silhouette over the crease criterion.
 */
class LIB_STROKE_EXPORT ChainSilhouetteIterator : public ChainingIterator
{
public:
	/*! Builds a ChainSilhouetteIterator from the first ViewEdge used for iteration and its orientation.
	 *  \param iRestrictToSelection
	 *    Indicates whether to force the chaining to stay within the set of selected ViewEdges or not.
	 *  \param begin
	 *    The ViewEdge from where to start the iteration.
	 *  \param orientation
	 *    If true, we'll look for the next ViewEdge among the ViewEdges that surround the ending ViewVertex of begin.
	 *    If false, we'll search over the ViewEdges surrounding the ending ViewVertex of begin.
	 */
	ChainSilhouetteIterator(bool iRestrictToSelection = true, ViewEdge *begin = NULL, bool orientation = true)
	: ChainingIterator(iRestrictToSelection, true, begin, orientation)
	{
	}

	/*! Copy constructor */
	ChainSilhouetteIterator(const ChainSilhouetteIterator& brother) : ChainingIterator(brother) {}

	/*! Returns the string "ChainSilhouetteIterator" */
	virtual string getExactTypeName() const
	{
		return "ChainSilhouetteIterator";
	}

	/*! This method iterates over the potential next ViewEdges and returns the one that will be followed next.
	 *  When reaching the end of a chain, 0 is returned.
	 */
	virtual int traverse(const AdjacencyIterator& it);

	/*! Inits the iterator context */ 
	virtual int init()
	{
		return 0;
	}
};

//
// ChainPredicateIterator
//
///////////////////////////////////////////////////////////

/*! A "generic" user-controlled ViewEdge iterator. This iterator is in particular built from a unary predicate and
 *  a binary predicate.
 *  First, the unary predicate is evaluated for all potential next ViewEdges in order to only keep the ones respecting
 *  a certain constraint.
 *  Then, the binary predicate is evaluated on the current ViewEdge together with each ViewEdge of the previous
 *  selection. The first ViewEdge respecting both the unary predicate and the binary predicate is kept as the next one.
 *  If none of the potential next ViewEdge respects these 2 predicates, 0 is returned.
 */
class LIB_STROKE_EXPORT ChainPredicateIterator : public ChainingIterator
{
protected:
	BinaryPredicate1D *_binary_predicate; // the caller is responsible for the deletion of this object
	UnaryPredicate1D  *_unary_predicate;  // the caller is responsible for the deletion of this object

public:
	/*! Builds a ChainPredicateIterator from a starting ViewEdge and its orientation.
	 *  \param iRestrictToSelection
	 *    Indicates whether to force the chaining to stay within the set of selected ViewEdges or not.
	 *  \param iRestrictToUnvisited
	 *    Indicates whether a ViewEdge that has already been chained must be ignored ot not.
	 *  \param begin
	 *    The ViewEdge from where to start the iteration.
	 *  \param orientation
	 *    If true, we'll look for the next ViewEdge among the ViewEdges that surround the ending ViewVertex of begin.
	 *    If false, we'll search over the ViewEdges surrounding the ending ViewVertex of begin.
	 */
	ChainPredicateIterator(bool iRestrictToSelection = true, bool iRestrictToUnvisited = true, ViewEdge *begin = NULL,
	                       bool orientation = true)
	: ChainingIterator(iRestrictToSelection, iRestrictToUnvisited, begin, orientation)
	{
		_binary_predicate = 0;
		_unary_predicate = 0;
	}

	/*! Builds a ChainPredicateIterator from a unary predicate, a binary predicate, a starting ViewEdge and
	 *  its orientation.
	 *  \param iRestrictToSelection
	 *    Indicates whether to force the chaining to stay within the set of selected ViewEdges or not.
	 *  \param iRestrictToUnvisited
	 *    Indicates whether a ViewEdge that has already been chained must be ignored ot not.
	 *  \param upred
	 *    The unary predicate that the next ViewEdge must satisfy.
	 *  \param bpred
	 *    The binary predicate that the next ViewEdge must satisfy together with the actual pointed ViewEdge.
	 *  \param begin
	 *    The ViewEdge from where to start the iteration.
	 *  \param orientation
	 *    If true, we'll look for the next ViewEdge among the ViewEdges that surround the ending ViewVertex of begin.
	 *    If false, we'll search over the ViewEdges surrounding the ending ViewVertex of begin.
	 */
	ChainPredicateIterator(UnaryPredicate1D& upred, BinaryPredicate1D& bpred, bool iRestrictToSelection = true,
	                       bool iRestrictToUnvisited = true, ViewEdge *begin = NULL, bool orientation = true)
	: ChainingIterator(iRestrictToSelection, iRestrictToUnvisited, begin, orientation)
	{
		_unary_predicate = &upred;
		_binary_predicate = &bpred;
	}

	/*! Copy constructor */
	ChainPredicateIterator(const ChainPredicateIterator& brother) : ChainingIterator(brother)
	{
		_unary_predicate = brother._unary_predicate;
		_binary_predicate = brother._binary_predicate;
	}

	/*! Destructor. */
	virtual ~ChainPredicateIterator()
	{
		_unary_predicate = 0;
		_binary_predicate = 0;
	}

	/*! Returns the string "ChainPredicateIterator" */
	virtual string getExactTypeName() const
	{
		return "ChainPredicateIterator";
	}

	/*! This method iterates over the potential next ViewEdges and returns the one that will be followed next.
	 *  When reaching the end of a chain, 0 is returned.
	 */
	virtual int traverse(const AdjacencyIterator &it);

	/*! Inits the iterator context */
	virtual int init()
	{
		return 0;
	}
};

} /* namespace Freestyle */

#endif // __FREESTYLE_CHAINING_ITERATORS_H__
