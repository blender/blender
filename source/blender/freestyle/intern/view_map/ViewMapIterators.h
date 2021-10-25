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

#ifndef __FREESTYLE_VIEW_MAP_ITERATORS_H__
#define __FREESTYLE_VIEW_MAP_ITERATORS_H__

/** \file blender/freestyle/intern/view_map/ViewMapIterators.h
 *  \ingroup freestyle
 *  \brief Iterators used to iterate over the various elements of the ViewMap
 *  \author Stephane Grabli
 *  \date 01/07/2003
 */

#include "ViewMap.h"

#include "../system/Iterator.h" //soc 

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

/*! Class representing an iterator over oriented ViewEdges around a ViewVertex. This iterator allows a CCW iteration
 *  (in the image plane).
 *  An instance of an orientedViewEdgeIterator can only be obtained from a ViewVertex by calling edgesBegin()
 *  or edgesEnd().
 */
class orientedViewEdgeIterator : public Iterator
{
public:
	friend class ViewVertex;
	friend class TVertex;
	friend class NonTVertex;
	friend class ViewEdge;

	// FIXME
	typedef TVertex::edge_pointers_container edge_pointers_container;
	typedef NonTVertex::edges_container edges_container;

protected:
	Nature::VertexNature _Nature; // the nature of the underlying vertex
	// T vertex attributes
	edge_pointers_container::iterator _tbegin;
	edge_pointers_container::iterator _tend;
	edge_pointers_container::iterator _tvertex_iter;

	// Non TVertex attributes
	edges_container::iterator _begin;
	edges_container::iterator _end;
	edges_container::iterator _nontvertex_iter;

public:
	/*! Default constructor */
	inline orientedViewEdgeIterator() {}

	inline orientedViewEdgeIterator(Nature::VertexNature iNature)
	{
		_Nature = iNature;
	}

	/*! Copy constructor */
	orientedViewEdgeIterator(const orientedViewEdgeIterator& iBrother)
	{
		_Nature = iBrother._Nature;
		if (_Nature & Nature::T_VERTEX) {
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

	virtual ~orientedViewEdgeIterator() {}

public:
	inline orientedViewEdgeIterator(edge_pointers_container::iterator begin, edge_pointers_container::iterator end,
	                                edge_pointers_container::iterator iter)
	{
		_Nature = Nature::T_VERTEX;
		_tbegin = begin;
		_tend = end;
		_tvertex_iter = iter;
	}

	inline orientedViewEdgeIterator(edges_container::iterator begin, edges_container::iterator end,
	                                edges_container::iterator iter)
	{
		_Nature = Nature::NON_T_VERTEX;
		_begin = begin;
		_end = end;
		_nontvertex_iter = iter;
	}

public:
	/*! Tells whether the ViewEdge pointed by this iterator is the first one of the iteration list or not. */
	virtual bool isBegin() const
	{
		if (_Nature & Nature::T_VERTEX)
			return (_tvertex_iter == _tbegin);
		else
			return (_nontvertex_iter == _begin);
	}

	/*! Tells whether the ViewEdge pointed by this iterator is after the last one of the iteration list or not. */
	virtual bool isEnd() const
	{
		if (_Nature & Nature::T_VERTEX)
			return (_tvertex_iter == _tend);
		else
			return (_nontvertex_iter == _end);
	}

	// operators
	/*! Increments. In the scripting language, call "increment()". */
	// operator corresponding to ++i
	virtual orientedViewEdgeIterator& operator++()
	{
		increment();
		return *this;
	}

	// operator corresponding to i++, i.e. which returns the value *and then* increments.
	// That's why we store the value in a temp.
	virtual orientedViewEdgeIterator operator++(int)
	{
		orientedViewEdgeIterator tmp = *this;
		increment();
		return tmp;
	}

	// comparibility
	/*! operator != */
	virtual bool operator!=(const orientedViewEdgeIterator& b) const
	{
		if (_Nature & Nature::T_VERTEX)
			return (_tvertex_iter != b._tvertex_iter);
		else
			return (_nontvertex_iter != b._nontvertex_iter);
	}

	/*! operator == */
	virtual bool operator==(const orientedViewEdgeIterator& b) const
	{
		return !(*this != b);
	}

	// dereferencing
	/*! Returns a reference to the pointed orientedViewEdge.
	 *  In the scripting language, you must call "getObject()" instead.
	 */
	virtual ViewVertex::directedViewEdge& operator*() const
	{
		if (_Nature & Nature::T_VERTEX)
			//return _tvertex_iter;
			return **_tvertex_iter;
		else
			return (*_nontvertex_iter);
	}
	/*! Returns a pointer to the pointed orientedViewEdge.
	 * Can't be called in the scripting language.
	 */
	virtual ViewVertex::directedViewEdge *operator->() const
	{
		return &(operator*());
	}

public:
	/*! increments.*/
	virtual inline int increment()
	{
		if (_Nature & Nature::T_VERTEX) {
			ViewVertex::directedViewEdge tmp = (**_tvertex_iter);
			++_tvertex_iter;
			if (_tvertex_iter != _tend) {
				// FIXME : pquoi deja ?
				ViewVertex::directedViewEdge tmp2 = (**_tvertex_iter);
				if (tmp2.first == tmp.first)
					++_tvertex_iter;
			}
		}
		else {
			++_nontvertex_iter;
		}
		return 0;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:orientedViewEdgeIterator")
#endif

};

}  // ViewVertexInternal namespace

/**********************************/
/*                                */
/*                                */
/*             ViewEdge           */
/*                                */
/*                                */
/**********************************/

namespace ViewEdgeInternal {

//
// SVertexIterator
//
/////////////////////////////////////////////////

class SVertexIterator : public Interface0DIteratorNested
{
public:
	SVertexIterator()
	{
		_vertex = NULL;
		_begin = NULL;
		_previous_edge = NULL;
		_next_edge = NULL;
		_t = 0;
	}

	SVertexIterator(const SVertexIterator& vi)
	{
		_vertex = vi._vertex;
		_begin = vi._begin;
		_previous_edge = vi._previous_edge;
		_next_edge = vi._next_edge;
		_t = vi._t;
	}

	SVertexIterator(SVertex *v, SVertex *begin, FEdge *prev, FEdge *next, float t)
	{
		_vertex = v;
		_begin = begin;
		_previous_edge = prev;
		_next_edge = next;
		_t = t;
	}

	SVertexIterator& operator=(const SVertexIterator& vi)
	{
		_vertex = vi._vertex;
		_begin = vi._begin;
		_previous_edge = vi._previous_edge;
		_next_edge = vi._next_edge;
		_t = vi._t;
		return *this;
	}

	virtual ~SVertexIterator() {}

	virtual string getExactTypeName() const
	{
		return "SVertexIterator";
	}

	virtual SVertex& operator*()
	{
		return *_vertex;
	}

	virtual SVertex *operator->()
	{
		return &(operator*());
	}

	virtual SVertexIterator& operator++()
	{
		increment();
		return *this;
	}

	virtual SVertexIterator operator++(int)
	{
		SVertexIterator ret(*this);
		increment();
		return ret;
	}

	virtual SVertexIterator& operator--()
	{
		decrement();
		return *this;
	}

	virtual SVertexIterator operator--(int)
	{
		SVertexIterator ret(*this);
		decrement();
		return ret;
	}

	virtual int increment()
	{
		if (!_next_edge) {
			_vertex = NULL;
			return 0;
		}
		_t += (float)_next_edge->getLength2D();
		_vertex = _next_edge->vertexB();
		_previous_edge = _next_edge;
		_next_edge = _next_edge->nextEdge();
		return 0;
	}

	virtual int decrement()
	{
		if (!_previous_edge) {
			_vertex = NULL;
			return 0;
		}
		if ((!_next_edge) && (!_vertex)) {
			_vertex = _previous_edge->vertexB();
			return 0;
		}
		_t -= (float)_previous_edge->getLength2D();
		_vertex = _previous_edge->vertexA();
		_next_edge = _previous_edge;
		_previous_edge = _previous_edge->previousEdge();
		return 0;
	}

	virtual bool isBegin() const
	{
		return _vertex == _begin;
	}

	virtual bool isEnd() const
	{
		return (!_vertex) || (_vertex == _begin && _previous_edge);
	}

	virtual float t() const
	{
		return _t;
	}

	virtual float u() const
	{
		return _t / (float)_next_edge->viewedge()->getLength2D();
	}

	virtual bool operator==(const Interface0DIteratorNested& it) const
	{
		const SVertexIterator *it_exact = dynamic_cast<const SVertexIterator*>(&it);
		if (!it_exact)
			return false;
		return (_vertex == it_exact->_vertex);
	}

	virtual SVertexIterator *copy() const
	{
		return new SVertexIterator(*this);
	}

private:
	SVertex *_vertex;
	SVertex *_begin;
	FEdge *_previous_edge;
	FEdge *_next_edge;
	float _t; // curvilinear abscissa

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SVertexIterator")
#endif

};


//
// ViewEdgeIterator (base class)
//
///////////////////////////////////////////////////////////

/*! Base class for iterators over ViewEdges of the ViewMap Graph.
 *  Basically the "increment()" operator of this class should be able to take the decision of "where" (on which
 *  ViewEdge) to go when pointing on a given ViewEdge.
 *  ::Caution::: the dereferencing operator returns a *pointer* to the pointed ViewEdge.
 */
class ViewEdgeIterator : public Iterator
{
public:
	/*! Builds a ViewEdgeIterator from a starting ViewEdge and its orientation.
	 *  \param begin
	 *    The ViewEdge from where to start the iteration.
	 *  \param orientation
	 *    If true, we'll look for the next ViewEdge among the ViewEdges that surround the ending ViewVertex of begin.
	 *    If false, we'll search over the ViewEdges surrounding the ending ViewVertex of begin.
	 */
	ViewEdgeIterator(ViewEdge *begin = NULL, bool orientation = true)
	{
		_orientation = orientation;
		_edge = begin;
		_begin = begin;
	}

	/*! Copy constructor */
	ViewEdgeIterator(const ViewEdgeIterator& it)
	{
		_orientation = it._orientation;
		_edge = it._edge;
		_begin = it._begin;
	}

	virtual ~ViewEdgeIterator() {}

	/*! Returns the string "ViewEdgeIterator" */
	virtual string getExactTypeName() const
	{
		return "ViewEdgeIterator";
	}

	/*! Returns the current pointed ViewEdge. */
	ViewEdge *getCurrentEdge()
	{
		return _edge;
	}

	/*! Sets the current pointed ViewEdge. */
	void setCurrentEdge(ViewEdge *edge)
	{
		_edge = edge;
	}

	/*! Returns the first ViewEdge used for the iteration. */
	ViewEdge *getBegin()
	{
		return _begin;
	}

	/*! Sets the first ViewEdge used for the iteration. */
	void setBegin(ViewEdge *begin)
	{
		_begin = begin;
	}

	/*! Gets the orientation of the pointed ViewEdge in the iteration. */
	bool getOrientation() const
	{
		return _orientation;
	}

	/*! Sets the orientation of the pointed ViewEdge in the iteration. */
	void setOrientation(bool orientation)
	{
		_orientation = orientation;
	}

	/*! Changes the current orientation. */
	void changeOrientation()
	{
		_orientation = !_orientation;
	}

	/*! Returns a *pointer* to the pointed ViewEdge. */
	virtual ViewEdge *operator*()
	{
		return _edge;
	}

	virtual ViewEdge *operator->()
	{
		return operator*();
	}

	/*! Increments. In the scripting language, call "increment()". */
	virtual ViewEdgeIterator& operator++()
	{
		increment();
		return *this;
	}

	/*! Increments. In the scripting language, call "increment()". */
	virtual ViewEdgeIterator operator++(int)
	{
		ViewEdgeIterator tmp(*this);
		increment();
		return tmp;
	}

	/*! increments. */
	virtual int increment()
	{
		cerr << "Warning: method increment() not implemented" << endl;
		return 0;
	}

	/*! Decrements. In the scripting language, call "decrement()". */
	virtual ViewEdgeIterator& operator--()
	{
		decrement();
		return *this;
	}

	/*! Decrements. In the scripting language, call "decrement()". */
	virtual ViewEdgeIterator operator--(int)
	{
		ViewEdgeIterator tmp(*this);
		decrement();
		return tmp;
	}

	/*! decrements. */
	virtual int decrement()
	{
		cerr << "Warning: method decrement() not implemented" << endl;
		return 0;
	}

	/*! Returns true if the pointed ViewEdge is the first one used for the iteration. */
	virtual bool isBegin() const
	{
		return _edge == _begin;
	}

	/*! Returns true if the pointed ViewEdge* equals 0. */
	virtual bool isEnd() const
	{
		return !_edge;
	}

	/*! operator == */
	virtual bool operator==(ViewEdgeIterator& it) const
	{
		return _edge == it._edge;
	}

	/*! operator != */
	virtual bool operator!=(ViewEdgeIterator& it) const
	{
		return !(*this == it);
	}

protected:
	bool _orientation;
	ViewEdge *_edge;
	ViewEdge *_begin;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ViewEdgeIterator")
#endif

};

} // end of namespace ViewEdgeInternal

} /* namespace Freestyle */

#endif // __FREESTYLE_VIEW_MAP_ITERATORS_H__
