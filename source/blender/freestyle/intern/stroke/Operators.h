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

#ifndef __FREESTYLE_OPERATORS_H__
#define __FREESTYLE_OPERATORS_H__

/** \file blender/freestyle/intern/stroke/Operators.h
 *  \ingroup freestyle
 *  \brief Class gathering stroke creation algorithms
 *  \author Stephane Grabli
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#include <iostream>
#include <vector>

#include "Chain.h"
#include "ChainingIterators.h"
#include "Predicates0D.h"
#include "Predicates1D.h"
#include "StrokeShader.h"

#include "../system/TimeStamp.h"

#include "../view_map/Interface1D.h"
#include "../view_map/ViewMap.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

/*! Class defining the operators used in a style module.
 *  There are 4 classes of operators: Selection, Chaining, Splitting and Creating. All these operators are
 *  user controlled in the scripting language through Functors, Predicates and Shaders that are taken as arguments.
 */
class Operators {

public:
	typedef vector<Interface1D*> I1DContainer;
	typedef vector<Stroke*> StrokesContainer;

	//
	// Operators
	//
	////////////////////////////////////////////////

	/*! Selects the ViewEdges of the ViewMap verifying a specified condition.
	 *  \param pred The predicate expressing this condition
	 */
	static int select(UnaryPredicate1D& pred);

	/*! Builds a set of chains from the current set of ViewEdges.
	 *  Each ViewEdge of the current list starts a new chain. The chaining operator then iterates over the ViewEdges
	 *  of the ViewMap using the user specified iterator.
	 *  This operator only iterates using the increment operator and is therefore unidirectional.
	 *  \param it
	 *           The iterator on the ViewEdges of the ViewMap. It contains the chaining rule.
	 *  \param pred
	 *           The predicate on the ViewEdge that expresses the stopping condition.
	 *  \param modifier
	 *           A function that takes a ViewEdge as argument and that is used to modify the processed ViewEdge
	 *           state (the timestamp incrementation is a typical illustration of such a modifier)
	 */
	static int chain(ViewEdgeInternal::ViewEdgeIterator& it, UnaryPredicate1D& pred, UnaryFunction1D_void& modifier);

	/*! Builds a set of chains from the current set of ViewEdges.
	 *  Each ViewEdge of the current list starts a new chain. The chaining operator then iterates over the ViewEdges
	 *  of the ViewMap using the user specified iterator.
	 *  This operator only iterates using the increment operator and is therefore unidirectional.
	 *  This chaining operator is different from the previous one because it doesn't take any modifier as argument.
	 *  Indeed, the time stamp (insuring that a ViewEdge is processed one time) is automatically managed in this case.
	 *  \param it
	 *           The iterator on the ViewEdges of the ViewMap. It contains the chaining rule.
	 *  \param pred
	 *           The predicate on the ViewEdge that expresses the stopping condition.
	 */
	static int chain(ViewEdgeInternal::ViewEdgeIterator& it, UnaryPredicate1D& pred);  

	/*! Builds a set of chains from the current set of ViewEdges.
	 *  Each ViewEdge of the current list potentially starts a new chain. The chaining operator then iterates over
	 *  the ViewEdges of the ViewMap using the user specified iterator.
	 *  This operator iterates both using the increment and decrement operators and is therefore bidirectional.
	 *  This operator works with a ChainingIterator which contains the chaining rules. It is this last one which can
	 *  be told to chain only edges that belong to the selection or not to process twice a ViewEdge during the chaining.
	 *  Each time a ViewEdge is added to a chain, its chaining time stamp is incremented. This allows you to keep track
	 *  of the number of chains to which a ViewEdge belongs to.
	 *  \param it
	 *           The ChainingIterator on the ViewEdges of the ViewMap. It contains the chaining rule.
	 *  \param pred
	 *           The predicate on the ViewEdge that expresses the stopping condition.
	 */
	static int bidirectionalChain(ChainingIterator& it, UnaryPredicate1D& pred);

	/*! The only difference with the above bidirectional chaining algorithm is that we don't need to pass a stopping
	 *  criterion. This might be desirable when the stopping criterion is already contained in the iterator definition.
	 *  Builds a set of chains from the current set of ViewEdges.
	 *  Each ViewEdge of the current list potentially starts a new chain. The chaining operator then iterates over
	 *  the ViewEdges of the ViewMap using the user specified iterator.
	 *  This operator iterates both using the increment and decrement operators and is therefore bidirectional.
	 *  This operator works with a ChainingIterator which contains the chaining rules. It is this last one which can be
	 *  told to chain only edges that belong to the selection or not to process twice a ViewEdge during the chaining.
	 *  Each time a ViewEdge is added to a chain, its chaining time stamp is incremented. This allows you to keep track
	 *  of the number of chains to which a ViewEdge belongs to.
	 *  \param it
	 *           The ChainingIterator on the ViewEdges of the ViewMap. It contains the chaining rule.
	 */
	static int bidirectionalChain(ChainingIterator& it);

	/*! Splits each chain of the current set of chains in a sequential way.
	 *  The points of each chain are processed (with a specified sampling) sequentially.
	 *  Each time a user specified starting condition is verified, a new chain begins and ends as soon as a
	 *  user-defined stopping predicate is verified.
	 *  This allows chains overlapping rather than chains partitioning.
	 *  The first point of the initial chain is the first point of one of the resulting chains.
	 *  The splitting ends when no more chain can start.
	 *  \param startingPred
	 *           The predicate on a point that expresses the starting condition
	 *  \param stoppingPred
	 *           The predicate on a point that expresses the stopping condition
	 *  \param sampling
	 *           The resolution used to sample the chain for the predicates evaluation. (The chain is not actually
	 *           resampled, a virtual point only progresses along the curve using this resolution)
	 */
	static int sequentialSplit(UnaryPredicate0D& startingPred, UnaryPredicate0D& stoppingPred, float sampling = 0.0f);

	/*! Splits each chain of the current set of chains in a sequential way.
	 *  The points of each chain are processed (with a specified sampling) sequentially and each time a user
	 *  specified condition is verified, the chain is split into two chains.
	 *  The resulting set of chains is a partition of the initial chain
	 *  \param pred
	 *           The predicate on a point that expresses the splitting condition
	 *  \param sampling
	 *           The resolution used to sample the chain for the predicate evaluation. (The chain is not actually
	 *           resampled, a virtual point only progresses along the curve using this resolution)
	 */
	static int sequentialSplit(UnaryPredicate0D& pred, float sampling = 0.0f);

	/*! Splits the current set of chains in a recursive way.
	 *  We process the points of each chain (with a specified sampling) to find the point minimizing a specified
	 *  function. The chain is split in two at this point and the two new chains are processed in the same way.
	 *  The recursivity level is controlled through a predicate 1D that expresses a stopping condition
	 *  on the chain that is about to be processed.
	 *  \param func
	 *           The Unary Function evaluated at each point of the chain.
	 *           The splitting point is the point minimizing this function
	 *  \param pred
	 *           The Unary Predicate ex pressing the recursivity stopping condition.
	 *           This predicate is evaluated for each curve before it actually gets split.
	 *           If pred(chain) is true, the curve won't be split anymore.
	 *  \param sampling
	 *           The resolution used to sample the chain for the predicates evaluation. (The chain is not actually
	 *           resampled, a virtual point only progresses along the curve using this resolution)
	 */
	static int recursiveSplit(UnaryFunction0D<double>& func, UnaryPredicate1D& pred, float sampling = 0);

	/*! Splits the current set of chains in a recursive way.
	 *  We process the points of each chain (with a specified sampling) to find the point minimizing a specified
	 *  function. The chain is split in two at this point and the two new chains are processed in the same way.
	 *  The user can specify a 0D predicate to make a first selection on the points that can potentially be split.
	 *  A point that doesn't verify the 0D predicate won't be candidate in realizing the min.
	 *  The recursivity level is controlled through a predicate 1D that expresses a stopping condition
	 *  on the chain that is about to be processed.
	 *  \param func
	 *           The Unary Function evaluated at each point of the chain.
	 *           The splitting point is the point minimizing this function
	 *  \param pred0d
	 *           The Unary Predicate 0D used to select the candidate points where the split can occur.
	 *           For example, it is very likely that would rather have your chain splitting around its middle point
	 *           than around one of its extremities. A 0D predicate working on the curvilinear abscissa allows
	 *           to add this kind of constraints.
	 *  \param pred
	 *           The Unary Predicate ex pressing the recursivity stopping condition.
	 *           This predicate is evaluated for each curve before it actually gets split.
	 *           If pred(chain) is true, the curve won't be split anymore.
	 *  \param sampling
	 *           The resolution used to sample the chain for the predicates evaluation. (The chain is not actually
	 *           resampled, a virtual point only progresses along the curve using this resolution)
	 */
	static int recursiveSplit(UnaryFunction0D<double>& func, UnaryPredicate0D& pred0d, UnaryPredicate1D& pred,
	                          float sampling = 0.0f);

	/*! Sorts the current set of chains (or viewedges) according to the comparison predicate given as argument.
	 *  \param pred
	 *           The binary predicate used for the comparison
	 */
	static int sort(BinaryPredicate1D& pred);

	/*! Creates and shades the strokes from the current set of chains.
	 *  A predicate can be specified to make a selection pass on the chains.
	 *  \param pred
	 *           The predicate that a chain must verify in order to be transform as a stroke
	 *  \param shaders
	 *           The list of shaders used to shade the strokes
	 */
	static int create(UnaryPredicate1D& pred, vector<StrokeShader*> shaders);

	//
	// Data access
	//
	////////////////////////////////////////////////

	static ViewEdge *getViewEdgeFromIndex(unsigned i)
	{
		return dynamic_cast<ViewEdge*>(_current_view_edges_set[i]);
	}

	static Chain *getChainFromIndex(unsigned i)
	{
		return dynamic_cast<Chain*>(_current_chains_set[i]);
	}

	static Stroke *getStrokeFromIndex(unsigned i)
	{
		return _current_strokes_set[i];
	}

	static unsigned getViewEdgesSize()
	{
		return _current_view_edges_set.size();
	}

	static unsigned getChainsSize()
	{
		return _current_chains_set.size();
	}

	static unsigned getStrokesSize()
	{
		return _current_strokes_set.size();
	}

	//
	// Not exported in Python
	//
	//////////////////////////////////////////////////

	static StrokesContainer *getStrokesSet()
	{
		return &_current_strokes_set;
	}

	static void reset(bool removeStrokes=true);

private:
	Operators() {}

	static I1DContainer _current_view_edges_set;
	static I1DContainer _current_chains_set;
	static I1DContainer *_current_set;
	static StrokesContainer _current_strokes_set;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Operators")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_OPERATORS_H__
