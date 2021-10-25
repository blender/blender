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

#ifndef __FREESTYLE_STROKE_ITERATORS_H__
#define __FREESTYLE_STROKE_ITERATORS_H__

/** \file blender/freestyle/intern/stroke/StrokeIterators.h
 *  \ingroup freestyle
 *  \brief Iterators used to iterate over the elements of the Stroke
 *  \author Stephane Grabli
 *  \date 01/07/2003
 */

#include "Stroke.h"

namespace Freestyle {

namespace StrokeInternal {

//
// StrokeVertexIterator
//
/////////////////////////////////////////////////

/*! Class defining an iterator designed to iterate over the StrokeVertex of a Stroke.
 *  An instance of a StrokeVertexIterator can only be obtained from a Stroke by calling strokeVerticesBegin() or
 *  strokeVerticesEnd().
 *  It is iterating over the same vertices as an Interface0DIterator.
 *  The difference resides in the object access. Indeed, an Interface0DIterator allows only an access to an
 *  Interface0D whereas we could need to access the specialized StrokeVertex type. In this case, one
 *  should use a StrokeVertexIterator.
 *  The castToInterface0DIterator() method is useful to get an Interface0DIterator from a StrokeVertexIterator in
 *  order to call any functions of the type UnaryFunction0D.
 *  \attention In the scripting language, you must call \code it2 = StrokeVertexIterator(it1) \endcode instead of
 *  \code it2 = it1 \endcode where \a it1 and \a it2 are 2 StrokeVertexIterator.
 *  Otherwise, incrementing \a it1 will also increment \a it2.
 */
class StrokeVertexIterator : public Interface0DIteratorNested
{
public:
	/*! Default constructor. */
	StrokeVertexIterator() {}

	/*! Copy constructor. */
	StrokeVertexIterator(const StrokeVertexIterator& vi)
	{
		_it = vi._it;
		_begin = vi._begin;
		_end = vi._end;
	}

	StrokeVertexIterator(const Stroke::vertex_container::iterator& it,
	                     const Stroke::vertex_container::iterator& begin,
	                     const Stroke::vertex_container::iterator& end)
	{
		_it = it;
		_begin = begin;
		_end = end;
	}

	virtual ~StrokeVertexIterator() {}

	/*! Casts this StrokeVertexIterator into an Interface0DIterator.
	 *  Useful for any call to a function of the type UnaryFunction0D.
	 */
	inline Interface0DIterator castToInterface0DIterator() const
	{
		Interface0DIterator ret(new StrokeVertexIterator(*this));
		return ret;
	}

	/*! operator=
	 *  \attention In the scripting language, you must call \code it2 = StrokeVertexIterator(it1) \endcode instead of
	 *  \code it2 = it1 \endcode where \a it1 and \a it2 are 2 StrokeVertexIterator.
	 *  Otherwise, incrementing \a it1 will also increment \a it2.
	 */
	StrokeVertexIterator& operator=(const StrokeVertexIterator& vi)
	{
		_it = vi._it;
		_begin = vi._begin;
		_end = vi._end;
		return *this;
	}

	/*! Returns the string "StrokeVertexIterator". */
	virtual string getExactTypeName() const
	{
		return "StrokeVertexIterator";
	}

	/*! Returns a reference to the pointed StrokeVertex.
	 *  In the scripting language, you must call "getObject()"instead.
	 */
	virtual StrokeVertex& operator*()
	{
		return **_it;
	}

	/*! Returns a pointer to the pointed StrokeVertex.
	 * Can't be called in the scripting language.
	 */
	virtual StrokeVertex *operator->()
	{
		return &(operator*());
	}

	/*! Increments. In the scripting language, call "increment()". */
	virtual StrokeVertexIterator& operator++()
	{
		increment();
		return *this;
	}

	/*! Increments. In the scripting language, call "increment()". */
	virtual StrokeVertexIterator operator++(int)
	{
		StrokeVertexIterator ret(*this);
		increment();
		return ret;
	}

	/*! Decrements. In the scripting language, call "decrement()". */
	virtual StrokeVertexIterator& operator--()
	{
		decrement();
		return *this;
	}

	/*! Decrements. In the scripting language, call "decrement()". */
	virtual StrokeVertexIterator operator--(int)
	{
		StrokeVertexIterator ret(*this);
		decrement();
		return ret;
	}

	/*! Increments. */
	virtual int increment()
	{
		++_it;
		return 0;
	}

	/*! Decrements. */
	virtual int decrement()
	{
		--_it;
		return 0;
	}

	/*! Returns true if the pointed StrokeVertex is the first of the Stroke. */
	bool isBegin() const
	{
		return _it == _begin;
	}

	/*! Returns true if the pointed StrokeVertex is the final valid StrokeVertex of the Stroke. */
	bool atLast()
	{
		if (_it == _end)
			return false;

		++_it;
		bool result = (_it == _end);
		--_it;
		return result;
	}

	/*! Returns true if the pointed StrokeVertex is after the last StrokeVertex of the Stroke. */
	bool isEnd() const
	{
		return _it == _end;
	}

	/*! operator == */
	virtual bool operator==(const Interface0DIteratorNested& it) const
	{
		const StrokeVertexIterator *it_exact = dynamic_cast<const StrokeVertexIterator *>(&it);
		if (!it_exact)
			return false;
		return (_it == it_exact->_it);
	}

	/*! Returns the curvilinear abscissa of the current point */
	virtual float t() const
	{
		return (*_it)->curvilinearAbscissa();
	}

	/*! Returns the point's parameter in the stroke */
	virtual float u() const
	{
		return (*_it)->u();
	}

	/*! Cloning method */
	virtual StrokeVertexIterator *copy() const
	{
		return new StrokeVertexIterator(*this);
	}

	//
	// Not exported in Python
	//
	//////////////////////////////////////////////////
	const Stroke::vertex_container::iterator& getIt()
	{
		return _it;
	}

private:
	Stroke::vertex_container::iterator _it;
	Stroke::vertex_container::iterator _begin;
	Stroke::vertex_container::iterator _end;
};

} // end of namespace StrokeInternal

} /* namespace Freestyle */

#endif // __FREESTYLE_STROKE_ITERATORS_H__
