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

#ifndef __FREESTYLE_CURVE_ITERATORS_H__
#define __FREESTYLE_CURVE_ITERATORS_H__

/** \file blender/freestyle/intern/stroke/CurveIterators.h
 *  \ingroup freestyle
 *  \brief Iterators used to iterate over the elements of the Curve
 *  \author Stephane Grabli
 *  \date 01/08/2003
 */

#include "Curve.h"
#include "Stroke.h"

namespace Freestyle {

namespace CurveInternal {

/*! iterator on a curve. Allows an iterating outside 
 *  initial vertices. A CurvePoint is instanciated an returned 
 *  when the iterator is dereferenced.
 */

class CurvePointIterator : public Interface0DIteratorNested
{ 
public:
	friend class Freestyle::Curve;

public:
	float _CurvilinearLength;
	float _step;
	Curve::vertex_container::iterator __A;
	Curve::vertex_container::iterator __B;
	Curve::vertex_container::iterator _begin;
	Curve::vertex_container::iterator _end;
	int _n;
	int _currentn;
	float _t;
	mutable CurvePoint _Point;
	float _CurveLength;

public:
	inline CurvePointIterator(float step = 0.0f) : Interface0DIteratorNested()
	{
		_step = step;
		_CurvilinearLength = 0.0f;
		_t = 0.0f;
		//_Point = 0;
		_n = 0;
		_currentn = 0;
		_CurveLength = 0;
	}

	inline CurvePointIterator(const CurvePointIterator& iBrother) : Interface0DIteratorNested()
	{
		__A = iBrother.__A;
		__B = iBrother.__B;
		_begin = iBrother._begin;
		_end = iBrother._end;
		_CurvilinearLength = iBrother._CurvilinearLength;
		_step = iBrother._step;
		_t = iBrother._t;
		_Point = iBrother._Point;
		_n = iBrother._n;
		_currentn = iBrother._currentn;
		_CurveLength = iBrother._CurveLength;
	}

	inline CurvePointIterator& operator=(const CurvePointIterator& iBrother)
	{
		__A = iBrother.__A;
		__B = iBrother.__B;
		_begin = iBrother._begin;
		_end = iBrother._end;
		_CurvilinearLength = iBrother._CurvilinearLength;
		_step = iBrother._step;
		_t = iBrother._t;
		_Point = iBrother._Point;
		_n = iBrother._n;
		_currentn = iBrother._currentn;
		_CurveLength = iBrother._CurveLength;
		return *this;
	}

	virtual ~CurvePointIterator() {}

protected:
	inline CurvePointIterator(Curve::vertex_container::iterator iA, Curve::vertex_container::iterator iB,
	                          Curve::vertex_container::iterator ibegin, Curve::vertex_container::iterator iend,
	                          int currentn, int n, float iCurveLength, float step, float t = 0.0f,
	                          float iCurvilinearLength = 0.0f)
	: Interface0DIteratorNested()
	{
		__A = iA;
		__B = iB;
		_begin = ibegin;
		_end = iend;
		_CurvilinearLength = iCurvilinearLength;
		_step = step;
		_t = t;
		_n = n;
		_currentn = currentn;
		_CurveLength = iCurveLength;
	}

public:
	virtual CurvePointIterator *copy() const
	{
		return new CurvePointIterator(*this);
	}

	inline Interface0DIterator castToInterface0DIterator() const
	{
		Interface0DIterator ret(new CurveInternal::CurvePointIterator(*this));
		return ret;
	}

	virtual string getExactTypeName() const
	{
		return "CurvePointIterator";
	}

	// operators
	inline CurvePointIterator& operator++()  // operator corresponding to ++i
	{
		increment();
		return *this;
	}

	inline CurvePointIterator& operator--()  // operator corresponding to --i
	{
		decrement();
		return *this;
	}

	// comparibility
	virtual bool operator==(const Interface0DIteratorNested& b) const
	{
		const CurvePointIterator *it_exact = dynamic_cast<const CurvePointIterator*>(&b);
		if (!it_exact)
			return false;
		return ((__A == it_exact->__A) && (__B == it_exact->__B) && (_t == it_exact->_t));
	}

	// dereferencing
	virtual CurvePoint& operator*()
	{
		return (_Point = CurvePoint(*__A, *__B, _t));
	}

	virtual CurvePoint *operator->()
	{
		return &(operator*());
	}

	virtual bool isBegin() const
	{
		if ((__A == _begin) && (_t < (float)M_EPSILON))
			return true;
		return false;
	}

	virtual bool isEnd() const
	{
		if (__B == _end)
			return true;
		return false;
	}

//protected:
	virtual int increment()
	{
		if ((_currentn == _n - 1) && (_t == 1.0f)) {
			// we're setting the iterator to end
			++__A;
			++__B;
			++_currentn;
			_t = 0.0f;
			return 0;
		}

		if (0 == _step) {  // means we iterate over initial vertices
			Vec3r vec_tmp((*__B)->point2d() - (*__A)->point2d());
			_CurvilinearLength += (float)vec_tmp.norm();
			if (_currentn == _n - 1) {
				_t = 1.0f;
				return 0;
			}
			++__B;
			++__A;
			++_currentn;
			return 0;
		}

		// compute the new position:
		Vec3r vec_tmp2((*__A)->point2d() - (*__B)->point2d());
		float normAB = (float)vec_tmp2.norm();

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
		return 0;
	}

	virtual int decrement()
	{
		if (_t == 0.0f) {  //we're at the beginning of the edge
			_t = 1.0f;
			--_currentn;
			--__A;
			--__B;
			if (_currentn == _n - 1)
				return 0;
		}

		if (0 == _step) {  // means we iterate over initial vertices
			Vec3r vec_tmp((*__B)->point2d() - (*__A)->point2d());
			_CurvilinearLength -= (float)vec_tmp.norm();
			_t = 0;
			return 0;
		}

		// compute the new position:
		Vec3r vec_tmp2((*__A)->point2d() - (*__B)->point2d());
		float normAB = (float)vec_tmp2.norm();

		if (normAB > M_EPSILON) {
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
		return 0;
	}

	virtual float t() const
	{
		return _CurvilinearLength;
	}

	virtual float u() const
	{
		return _CurvilinearLength / _CurveLength;
	}
};

} // end of namespace CurveInternal

} /* namespace Freestyle */

#endif // __FREESTYLE_CURVE_ITERATORS_H__
