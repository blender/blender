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

#ifndef __FREESTYLE_CURVE_ADVANCED_ITERATORS_H__
#define __FREESTYLE_CURVE_ADVANCED_ITERATORS_H__

/** \file blender/freestyle/intern/stroke/CurveAdvancedIterators.h
 *  \ingroup freestyle
 *  \brief Iterators used to iterate over the elements of the Curve. Can't be used in python
 *  \author Stephane Grabli
 *  \date 01/08/2003
 */

#include "Stroke.h"

namespace Freestyle {

namespace CurveInternal {

class CurvePoint_const_traits : public Const_traits<CurvePoint*>
{
public:
	typedef deque<CurvePoint*> vertex_container;
	typedef vertex_container::const_iterator vertex_container_iterator;
	typedef SVertex vertex_type; 
};

class CurvePoint_nonconst_traits : public Nonconst_traits<CurvePoint*>
{
public:
	typedef deque<CurvePoint*> vertex_container;
	typedef vertex_container::iterator vertex_container_iterator;
	typedef SVertex vertex_type; 
};

/**********************************/
/*                                */
/*                                */
/*     CurvePoint Iterator        */
/*                                */
/*                                */
/**********************************/


/*! iterator on a curve. Allows an iterating outside  initial vertices. A CurvePoint is instanciated an returned
 *  when the iterator is dereferenced.
 */
template<class Traits>
class __point_iterator : public IteratorBase<Traits, BidirectionalIteratorTag_Traits>
{
public:
	typedef __point_iterator <Traits> Self;
	typedef typename Traits::vertex_container_iterator vertex_container_iterator;
	typedef typename Traits::vertex_type vertex_type;
	typedef CurvePoint Point;
	typedef Point point_type;

	typedef __point_iterator<CurvePoint_nonconst_traits> iterator;
	typedef __point_iterator<CurvePoint_const_traits> const_iterator;

#if 0
	typedef Vertex vertex_type ;
	typedef vertex_container_iterator vertex_iterator_type;
	typedef CurvePoint<Vertex> Point;
	typedef Point point_type;
#endif
	typedef IteratorBase<Traits, BidirectionalIteratorTag_Traits> parent_class;
#if 0
#if defined(__GNUC__) && (__GNUC__ < 3)
	typedef bidirectional_iterator<CurvePoint<Vertex>, ptrdiff_t> bidirectional_point_iterator;
#else
	typedef iterator<bidirectional_iterator_tag, CurvePoint<Vertex>, ptrdiff_t> bidirectional_point_iterator;
#endif
#endif
	friend class Curve;
#if 0
	friend class Curve::vertex_iterator;
	friend class __point_iterator<CurvePoint_nonconst_traits>;
	friend class iterator;
#endif
//protected:
public:
	float _CurvilinearLength;
	float _step;
	vertex_container_iterator __A;
	vertex_container_iterator __B;
	vertex_container_iterator _begin;
	vertex_container_iterator _end;
	int _n;
	int _currentn;
	float _t;
	mutable Point *_Point;

public:
	inline __point_iterator(float step = 0.0f) : parent_class()
	{
		_step = step;
		_CurvilinearLength = 0.0f;
		_t = 0.0f;
		_Point = 0;
		_n = 0;
		_currentn = 0;
	}

	inline __point_iterator(const iterator& iBrother) : parent_class()
	{
		__A = iBrother.__A;
		__B = iBrother.__B;
		_begin = iBrother._begin;
		_end = iBrother._end;
		_CurvilinearLength = iBrother._CurvilinearLength;
		_step = iBrother._step;
		_t = iBrother._t;
		if (iBrother._Point == 0)
			_Point = 0;
		else
			_Point = new Point(*(iBrother._Point));
		_n = iBrother._n;
		_currentn = iBrother._currentn;
	}

	inline __point_iterator(const const_iterator& iBrother) : parent_class()
	{
		__A = iBrother.__A;
		__B = iBrother.__B;
		_begin = iBrother._begin;
		_end = iBrother._end;
		_CurvilinearLength = iBrother._CurvilinearLength;
		_step = iBrother._step;
		_t = iBrother._t;
		if (iBrother._Point == 0)
			_Point = 0;
		else
			_Point = new Point(*(iBrother._Point));
		_n = iBrother._n;
		_currentn = iBrother._currentn;
	}

	inline Self& operator=(const Self& iBrother)
	{
		//((bidirectional_point_iterator*)this)->operator=(iBrother);
		__A = iBrother.__A;
		__B = iBrother.__B;
		_begin = iBrother._begin;
		_end = iBrother._end;
		_CurvilinearLength = iBrother._CurvilinearLength;
		_step = iBrother._step;
		_t = iBrother._t;
		if (iBrother._Point == 0)
			_Point = 0;
		else
			_Point = new Point(*(iBrother._Point));
		_n = iBrother._n;
		_currentn = iBrother._currentn;
		return *this;
	}

	virtual ~__point_iterator()
	{
		if (_Point != 0)
			delete _Point;
	}

//protected:  //FIXME
public:
	inline __point_iterator(vertex_container_iterator iA, vertex_container_iterator iB,
	                        vertex_container_iterator ibegin, vertex_container_iterator iend,
	                        int currentn, int n, float step, float t = 0.0f, float iCurvilinearLength = 0.0f)
	: parent_class()
	{
		__A = iA;
		__B = iB;
		_begin = ibegin;
		_end = iend;
		_CurvilinearLength = iCurvilinearLength;
		_step = step;
		_t = t;
		_Point = 0;
		_n = n;
		_currentn = currentn;
	}

public:
	// operators
	inline Self& operator++()  // operator corresponding to ++i
	{
		increment();
		return *this;
	}

	/* Operator corresponding to i++, i.e. it returns the value *and then* increments.
	 * That’s why we store the value in a temp.
	 */
	inline Self operator++(int)
	{
		Self tmp = *this;
		increment();
		return tmp;
	}

	inline Self& operator--()  // operator corresponding to --i
	{
		decrement();
		return *this;
	}

	inline Self operator--(int)  // operator corresponding to i--
	{
		Self tmp = *this;
		decrement();
		return tmp;
	}

	// comparibility
	virtual bool operator!=(const Self& b) const
	{
		return ((__A != b.__A) || (__B != b.__B) || (_t != b._t));
	}

	virtual bool operator==(const Self& b) const
	{
		return !(*this != b);
	}

	// dereferencing
	virtual typename Traits::reference operator*() const
	{
		if (_Point != 0) {
			delete _Point;
			_Point = 0;
		}
		if ((_currentn < 0) || (_currentn >= _n))
			return _Point; // 0 in this case
		return (_Point = new Point(*__A, *__B, _t));
	}

	virtual typename Traits::pointer operator->() const
	{
		return &(operator*());
	}

	virtual bool begin() const
	{
		if ((__A == _begin) && (_t < (float)M_EPSILON))
			return true;
		return false;
	}

	virtual bool end() const
	{
		if ((__B == _end))
			return true;
		return false;
	}

protected:
	virtual void increment()
	{
		if (_Point != 0) {
			delete _Point;
		_Point = 0;
		}
		if ((_currentn == _n - 1) && (_t == 1.0f)) {
			// we're setting the iterator to end
			++__A;
			++__B;
			++_currentn;
			_t = 0.0f;
			return;
		}

		if (0 == _step) {  // means we iterate over initial vertices
			Vec3r vec_tmp((*__B)->point2d() - (*__A)->point2d());
			_CurvilinearLength += vec_tmp.norm();
			if (_currentn == _n - 1) {
				_t = 1.0f;
				return;
			}
			++__B;
			++__A;
			++_currentn;
			return;
		}

		// compute the new position:
		Vec3r vec_tmp2((*__A)->point2d() - (*__B)->point2d());
		float normAB = vec_tmp2.norm();

		if (normAB > M_EPSILON) {
			_CurvilinearLength += _step;
			_t = _t + _step / normAB;
		}
		else {
			_t = 1.0f; // AB is a null segment, we're directly at its end
		}
		//if normAB ~= 0, we don't change these values
		if (_t >= 1) {
			_CurvilinearLength -= normAB * (_t - 1);
			if (_currentn == _n - 1) {
				_t = 1.0f;
			}
			else {
				_t = 0.0f;
				++_currentn;
				++__A;
				++__B;
			}
		}
	}

	virtual void decrement() 
	{
		if (_Point != 0) {
			delete _Point;
			_Point = 0;
		}

		if (_t == 0.0f) {  // we're at the beginning of the edge
			_t = 1.0f;
			--_currentn;
			--__A;
			--__B;
			if (_currentn == _n - 1)
				return;
		}

		if (0 == _step) {  // means we iterate over initial vertices
			Vec3r vec_tmp((*__B)->point2d() - (*__A)->point2d());
			_CurvilinearLength -= vec_tmp.norm();
			_t = 0;
			return;
		}

		// compute the new position:
		Vec3r vec_tmp2((*__A)->point2d() - (*__B)->point2d());
		float normAB = vec_tmp2.norm();

		if (normAB >M_EPSILON) {
			_CurvilinearLength -= _step;
			_t = _t - _step / normAB;
		}
		else {
			_t = -1.0f; // We just need a negative value here
		}

		// round value
		if (fabs(_t) < (float)M_EPSILON)
			_t = 0.0f;
		if (_t < 0) {
			if (_currentn == 0)
				_CurvilinearLength = 0.0f;
			else
				_CurvilinearLength += normAB * (-_t);
			_t = 0.0f;
		}
	}
};

} // end of namespace CurveInternal

} /* namespace Freestyle */

#endif // __FREESTYLE_CURVE_ADVANCED_ITERATORS_H__
