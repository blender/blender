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

/** \file blender/freestyle/intern/stroke/Curve.cpp
 *  \ingroup freestyle
 *  \brief Class to define a container for curves
 *  \author Stephane Grabli
 *  \date 11/01/2003
 */

#include "Curve.h"
#include "CurveAdvancedIterators.h"
#include "CurveIterators.h"

#include "BKE_global.h"

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*             CurvePoint         */
/*                                */
/*                                */
/**********************************/

CurvePoint::CurvePoint()
{
	__A = 0;
	__B = 0;
	_t2d = 0;
}

CurvePoint::CurvePoint(SVertex *iA, SVertex *iB, float t)
{
	__A = iA;
	__B = iB;
	_t2d = t;
	if ((iA == 0) && (t == 1.0f)) {
		_Point2d = __B->point2d();
		_Point3d = __B->point3d();
	}
	else if ((iB == 0) && (t == 0.0f)) {
		_Point2d = __A->point2d();
		_Point3d = __A->point3d();
	}
	else {
		_Point2d = __A->point2d() + _t2d * (__B->point2d() - __A->point2d());
		_Point3d = __A->point3d() + _t2d * (__B->point3d() - __A->point3d());
	}
}

CurvePoint::CurvePoint(CurvePoint *iA, CurvePoint *iB, float t3)
{
	__A = 0;
	__B = 0;
	float t1 = iA->t2d();
	float t2 = iB->t2d();
	if ((iA->A() == iB->A()) && (iA->B() == iB->B()) &&
	    (iA->A() != 0) && (iA->B() != 0) && (iB->A() != 0) && (iB->B() != 0))
	{
		__A = iA->A();
		__B = iB->B();
		_t2d = t1 + t2 * t3 - t1 * t3;
	}
	else if ((iA->B() == 0) && (iB->B() == 0)) {
		__A = iA->A();
		__B = iB->A();
		_t2d = t3;
	}
	else if ((iA->t2d() == 0) && (iB->t2d() == 0)) {
		__A = iA->A();
		__B = iB->A();
		_t2d = t3;
	}
	else if (iA->A() == iB->A()) {
iA_A_eq_iB_A:
		if (iA->t2d() == 0) {
			__A = iB->A();
			__B = iB->B();
			_t2d = t3;
		}
		else if (iB->t2d() == 0) {
			__A = iA->A();
			__B = iA->B();
			_t2d = t3;
		}
	}
	else if (iA->B() == iB->B()) {
iA_B_eq_iB_B:
		if (iA->t2d() == 1) {
			__A = iB->A();
			__B = iB->B();
			_t2d = t3;
		}
		else if (iB->t2d() == 1) {
			__A = iA->A();
			__B = iA->B();
			_t2d = t3;
		}
	}
	else if (iA->B() == iB->A()) {
iA_B_eq_iB_A:
		if ((iA->t2d() != 1.0f) && (iB->t2d() == 0.0f)) {
			__A = iA->A();
			__B = iA->B();
			_t2d = t1 + t3 - t1 * t3;
			//_t2d = t3;
		}
		else if ((iA->t2d() == 1.0f) && (iB->t2d() != 0.0f)) {
			__A = iB->A();
			__B = iB->B();
			//_t2d = t3;
			_t2d = t2 * t3;
		}
		else if ((iA->getPoint2D() - iA->getPoint2D()).norm() < 1.0e-6) {
			__A = iB->A();
			__B = iB->B();
			//_t2d = t3;
			_t2d = t2 * t3;
		}
	}
	else if (iA->A() != 0 && iB->A() != 0 && (iA->A()->point3d() - iB->A()->point3d()).norm() < 1.0e-6) {
		goto iA_A_eq_iB_A;
	}
	else if (iA->B() != 0 && iB->B() != 0 && (iA->B()->point3d() - iB->B()->point3d()).norm() < 1.0e-6) {
		goto iA_B_eq_iB_B;
	}
	else if (iA->B() != 0 && iB->A() != 0 && (iA->B()->point3d() - iB->A()->point3d()).norm() < 1.0e-6) {
		goto iA_B_eq_iB_A;
	}

	if (!__A || !__B) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			printf("iA A 0x%p p (%f, %f)\n", iA->A(), iA->A()->getPoint2D().x(), iA->A()->getPoint2D().y());
			printf("iA B 0x%p p (%f, %f)\n", iA->B(), iA->B()->getPoint2D().x(), iA->B()->getPoint2D().y());
			printf("iB A 0x%p p (%f, %f)\n", iB->A(), iB->A()->getPoint2D().x(), iB->A()->getPoint2D().y());
			printf("iB B 0x%p p (%f, %f)\n", iB->B(), iB->B()->getPoint2D().x(), iB->B()->getPoint2D().y());
			printf("iA t2d %f p (%f, %f)\n", iA->t2d(), iA->getPoint2D().x(), iA->getPoint2D().y());
			printf("iB t2d %f p (%f, %f)\n", iB->t2d(), iB->getPoint2D().x(), iB->getPoint2D().y());
		}
		cerr << "Fatal error in CurvePoint::CurvePoint(CurvePoint *iA, CurvePoint *iB, float t3)" << endl;
	}
	assert(__A != 0 && __B != 0);

#if 0
	_Point2d = __A->point2d() + _t2d * (__B->point2d() - __A->point2d());
	_Point3d = __A->point3d() + _t2d * (__B->point3d() - __A->point3d());
#endif

	_Point2d = iA->point2d() + t3 * (iB->point2d() - iA->point2d());
	_Point3d = __A->point3d() + _t2d * (__B->point3d() - __A->point3d());
}

CurvePoint::CurvePoint(const CurvePoint& iBrother)
{
	__A = iBrother.__A;
	__B = iBrother.__B;
	_t2d = iBrother._t2d;
	_Point2d = iBrother._Point2d;
	_Point3d = iBrother._Point3d;
}

CurvePoint& CurvePoint::operator=(const CurvePoint& iBrother)
{
	__A = iBrother.__A;
	__B = iBrother.__B;
	_t2d = iBrother._t2d;
	_Point2d = iBrother._Point2d;
	_Point3d = iBrother._Point3d;
	return *this;
}


FEdge *CurvePoint::fedge()
{
	if (getNature() & Nature::T_VERTEX)
		return 0;
	return __A->fedge();
}


FEdge *CurvePoint::getFEdge(Interface0D& inter)
{
	CurvePoint *iVertexB = dynamic_cast<CurvePoint*>(&inter);
	if (!iVertexB) {
		cerr << "Warning: CurvePoint::getFEdge() failed to cast the given 0D element to CurvePoint." << endl;
		return 0;
	}
	if (((__A == iVertexB->__A) && (__B == iVertexB->__B)) ||
	    ((__A == iVertexB->__B) && (__B == iVertexB->__A)))
	{
		return __A->getFEdge(*__B);
	}
	if (__B == 0) {
		if (iVertexB->__B == 0)
			return __A->getFEdge(*(iVertexB->__A));
		else if (iVertexB->__A == __A)
			return __A->getFEdge(*(iVertexB->__B));
		else if (iVertexB->__B == __A)
			return __A->getFEdge(*(iVertexB->__A));
	}
	if (iVertexB->__B == 0) {
		if (iVertexB->__A == __A)
			return __B->getFEdge(*(iVertexB->__A));
		else if (iVertexB->__A == __B)
			return __A->getFEdge(*(iVertexB->__A));
	}
	if (__B == iVertexB->__A) {
		if ((_t2d != 1) && (iVertexB->_t2d == 0))
			return __A->getFEdge(*__B);
		if ((_t2d == 1) && (iVertexB->_t2d != 0))
			return iVertexB->__A->getFEdge(*(iVertexB->__B));
	}
	if (__B == iVertexB->__B) {
		if ((_t2d != 1) && (iVertexB->_t2d == 1))
			return __A->getFEdge(*__B);
		if ((_t2d == 1) && (iVertexB->_t2d != 1))
			return iVertexB->__A->getFEdge(*(iVertexB->__B));
	}
	if (__A == iVertexB->__A) {
		if ((_t2d == 0) && (iVertexB->_t2d != 0))
			return iVertexB->__A->getFEdge(*(iVertexB->__B));
		if ((_t2d != 0) && (iVertexB->_t2d == 0))
			return __A->getFEdge(*__B);
	}
	if (__A == iVertexB->__B) {
		if ((_t2d == 0) && (iVertexB->_t2d != 1))
			return iVertexB->__A->getFEdge(*(iVertexB->__B));
		if ((_t2d != 0) && (iVertexB->_t2d == 1))
			return __A->getFEdge(*__B);
	}
#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		printf("__A           0x%p p (%f, %f)\n", __A, __A->getPoint2D().x(), __A->getPoint2D().y());
		printf("__B           0x%p p (%f, %f)\n", __B, __B->getPoint2D().x(), __B->getPoint2D().y());
		printf("iVertexB->A() 0x%p p (%f, %f)\n", iVertexB->A(), iVertexB->A()->getPoint2D().x(),
		                                          iVertexB->A()->getPoint2D().y());
		printf("iVertexB->B() 0x%p p (%f, %f)\n", iVertexB->B(), iVertexB->B()->getPoint2D().x(),
		                                          iVertexB->B()->getPoint2D().y());
		printf("_t2d            %f p (%f, %f)\n", _t2d, getPoint2D().x(), getPoint2D().y());
		printf("iVertexB->t2d() %f p (%f, %f)\n", iVertexB->t2d(), iVertexB->getPoint2D().x(),
		       iVertexB->getPoint2D().y());
	}
#endif
	cerr << "Warning: CurvePoint::getFEdge() failed." << endl;

	return NULL;
}


Vec3r CurvePoint::normal() const
{
	if (__B == 0)
		return __A->normal();
	if (__A == 0)
		return __B->normal();
	Vec3r Na = __A->normal();
	if (Exception::getException())
		Na = Vec3r(0, 0, 0);
	Vec3r Nb = __B->normal();
	if (Exception::getException())
		Nb = Vec3r(0, 0, 0);
	// compute t3d:
	real t3d = SilhouetteGeomEngine::ImageToWorldParameter(__A->getFEdge(*__B), _t2d);
	return ((1 - t3d) * Na + t3d * Nb);
}


#if 0
Material CurvePoint::material() const
{
	if (__A == 0)
		return __B->material();
	return __A->material();
}

Id CurvePoint::shape_id() const
{
	if (__A == 0)
		return __B->shape_id();
	return __A->shape_id();
}
#endif


const SShape *CurvePoint::shape() const
{
	if (__A == 0)
		return __B->shape();
	return __A->shape();
}

#if 0
float CurvePoint::shape_importance() const
{
	if (__A == 0)
		return __B->shape_importance();
	return __A->shape_importance();
} 


const unsigned CurvePoint::qi() const
{
	if (__A == 0)
		return __B->qi();
	if (__B == 0)
		return __A->qi();
	return __A->getFEdge(*__B)->qi();
}
#endif

occluder_container::const_iterator CurvePoint::occluders_begin() const
{
	if (__A == 0)
		return __B->occluders_begin();
	if (__B == 0)
		return __A->occluders_begin();
	return __A->getFEdge(*__B)->occluders_begin();
}

occluder_container::const_iterator CurvePoint::occluders_end() const
{
	if (__A == 0)
		return __B->occluders_end();
	if (__B == 0)
		return __A->occluders_end();
	return __A->getFEdge(*__B)->occluders_end();
}

bool CurvePoint::occluders_empty() const
{
	if (__A == 0)
		return __B->occluders_empty();
	if (__B == 0)
		return __A->occluders_empty();
	return __A->getFEdge(*__B)->occluders_empty();
}

int CurvePoint::occluders_size() const
{
	if (__A == 0)
		return __B->occluders_size();
	if (__B == 0)
		return __A->occluders_size();
	return __A->getFEdge(*__B)->occluders_size();
}

const SShape *CurvePoint::occluded_shape() const
{
	if (__A == 0)
		return __B->occluded_shape();
	if (__B == 0)
		return __A->occluded_shape();
	return __A->getFEdge(*__B)->occluded_shape();
}

const Polygon3r& CurvePoint::occludee() const
{
	if (__A == 0)
		return __B->occludee();
	if (__B == 0)
		return __A->occludee();
	return __A->getFEdge(*__B)->occludee();
}

const bool  CurvePoint::occludee_empty() const
{
	if (__A == 0)
		return __B->occludee_empty();
	if (__B == 0)
		return __A->occludee_empty();
	return __A->getFEdge(*__B)->occludee_empty();
}

real CurvePoint::z_discontinuity() const
{
	if (__A == 0)
		return __B->z_discontinuity();
	if (__B == 0)
		return __A->z_discontinuity();
	if (__A->getFEdge(*__B) == 0)
		return 0.0;

	return __A->getFEdge(*__B)->z_discontinuity();
}

#if 0
float CurvePoint::local_average_depth() const
{
	return local_average_depth_function<CurvePoint >(this);
}

float CurvePoint::local_depth_variance() const
{
	return local_depth_variance_function<CurvePoint >(this);
}

real CurvePoint::local_average_density(float sigma) const
{
	//return local_average_density<CurvePoint >(this);
	return density_function<CurvePoint >(this);
}

Vec3r shaded_color() const;

Vec3r CurvePoint::orientation2d() const
{
	if (__A == 0)
		return __B->orientation2d();
	if (__B == 0)
		return __A->orientation2d();
	return __B->point2d() - __A->point2d();
}

Vec3r CurvePoint::orientation3d() const
{
	if (__A == 0)
		return __B->orientation3d();
	if (__B == 0)
		return __A->orientation3d();
	return __B->point3d() - __A->point3d();
}

real curvature2d() const
{
	return viewedge()->curvature2d((_VertexA->point2d() + _VertexB->point2d()) / 2.0);
}

Vec3r CurvePoint::curvature2d_as_vector() const
{
#if 0
	Vec3r edgeA = (_FEdges[0])->orientation2d().normalize();
	Vec3r edgeB = (_FEdges[1])->orientation2d().normalize();
	return edgeA + edgeB;
#endif
	if (__A == 0)
		return __B->curvature2d_as_vector();
	if (__B == 0)
		return __A->curvature2d_as_vector();
	return ((1 - _t2d) * __A->curvature2d_as_vector() + _t2d * __B->curvature2d_as_vector());
}

real CurvePoint::curvature2d_as_angle() const
{
#if 0
	Vec3r edgeA = (_FEdges[0])->orientation2d();
	Vec3r edgeB = (_FEdges[1])->orientation2d();
	Vec2d N1(-edgeA.y(), edgeA.x());
	N1.normalize();
	Vec2d N2(-edgeB.y(), edgeB.x());
	N2.normalize();
	return acos((N1 * N2));
#endif
	if (__A == 0)
		return __B->curvature2d_as_angle();
	if (__B == 0)
		return __A->curvature2d_as_angle();
	return ((1 - _t2d) * __A->curvature2d_as_angle() + _t2d * __B->curvature2d_as_angle());
}
#endif

real CurvePoint::curvatureFredo() const
{
	if (__A == 0)
		return __B->curvatureFredo();
	if (__B == 0)
		return __A->curvatureFredo();
	return ((1 - _t2d) * __A->curvatureFredo() + _t2d * __B->curvatureFredo());
}

Vec2d CurvePoint::directionFredo () const
{
	if (__A == 0)
		return __B->directionFredo();
	if (__B == 0)
		return __A->directionFredo();
	return ((1 - _t2d) * __A->directionFredo() + _t2d * __B->directionFredo());
}

/**********************************/
/*                                */
/*                                */
/*             Curve              */
/*                                */
/*                                */
/**********************************/

/* for  functions */

Curve::~Curve()
{
	if (!_Vertices.empty()) {
		for (vertex_container::iterator it = _Vertices.begin(), itend = _Vertices.end(); it != itend; ++it) {
			delete (*it);
		}
		_Vertices.clear();
	}
}

/*! iterators access */
Curve::point_iterator Curve::points_begin(float step)
{
	vertex_container::iterator second = _Vertices.begin();
	++second;
	return point_iterator(_Vertices.begin(), second, _Vertices.begin(), _Vertices.end(), _nSegments, step, 0.0f, 0.0f);
	//return point_iterator(_Vertices.begin(), second, _nSegments, step, 0.0f, 0.0f);
}

Curve::const_point_iterator Curve::points_begin(float step) const
{
	vertex_container::const_iterator second = _Vertices.begin();
	++second;
	return const_point_iterator(_Vertices.begin(), second, _Vertices.begin(), _Vertices.end(),
	                            _nSegments, step, 0.0f, 0.0f);
	//return const_point_iterator(_Vertices.begin(), second, _nSegments, step, 0.0f, 0.0f);
}

Curve::point_iterator Curve::points_end(float step)
{
	return point_iterator(_Vertices.end(), _Vertices.end(), _Vertices.begin(), _Vertices.end(),
	                      _nSegments, step, 1.0f, _Length);
	//return point_iterator(_Vertices.end(), _Vertices.end(), _nSegments, step, 1.0f, _Length);
}

Curve::const_point_iterator Curve::points_end(float step) const
{
	return const_point_iterator(_Vertices.end(), _Vertices.end(), _Vertices.begin(), _Vertices.end(),
	                            _nSegments, step, 1.0f, _Length);
	//return const_point_iterator(_Vertices.end(), _Vertices.end(), _nSegments, step, 1.0f, _Length);
}

// Adavnced Iterators access
Curve::point_iterator Curve::vertices_begin()
{
	return points_begin(0);
}

Curve::const_point_iterator Curve::vertices_begin() const
{
	return points_begin(0);
}

Curve::point_iterator Curve::vertices_end()
{
	return points_end(0);
}

Curve::const_point_iterator Curve::vertices_end() const
{
	return points_end(0);
}

// specialized iterators access
CurveInternal::CurvePointIterator Curve::curvePointsBegin(float t)
{
	vertex_container::iterator second = _Vertices.begin();
	++second;
	return CurveInternal::CurvePointIterator(_Vertices.begin(), second, _Vertices.begin(), _Vertices.end(),
	                                         0, _nSegments, _Length, t, 0.0f, 0.0f);
}

CurveInternal::CurvePointIterator Curve::curvePointsEnd(float t)
{
	vertex_container::iterator last = _Vertices.end();
	--last;
	return CurveInternal::CurvePointIterator(last, _Vertices.end(), _Vertices.begin(), _Vertices.end(),
	                                         _nSegments, _nSegments, _Length, t, 0.0f, _Length);
}

CurveInternal::CurvePointIterator  Curve::curveVerticesBegin()
{
	return curvePointsBegin(0);
}

CurveInternal::CurvePointIterator Curve::curveVerticesEnd()
{
	return curvePointsEnd(0);
}

Interface0DIterator Curve::pointsBegin(float t)
{
	vertex_container::iterator second = _Vertices.begin();
	++second;
	Interface0DIterator ret(new CurveInternal::CurvePointIterator(_Vertices.begin(), second, _Vertices.begin(),
	                                                              _Vertices.end(), 0, _nSegments, _Length,
	                                                              t, 0.0f, 0.0f));
	return ret;
}

Interface0DIterator Curve::pointsEnd(float t)
{
	vertex_container::iterator last = _Vertices.end();
	--last;
	Interface0DIterator ret(new CurveInternal::CurvePointIterator(last, _Vertices.end(), _Vertices.begin(),
	                                                              _Vertices.end(), _nSegments, _nSegments,
	                                                              _Length, t, 0.0f, _Length));
	return ret;
}

Interface0DIterator Curve::verticesBegin()
{
	return pointsBegin(0);
}

Interface0DIterator Curve::verticesEnd()
{
	return pointsEnd(0);
}

#if 0
Vec3r shaded_color(int iCombination = 0) const;

Vec3r Curve::orientation2d(point_iterator it) const
{
	return (*it)->orientation2d();
}

template <class BaseVertex>
Vec3r Curve::orientation2d(int iCombination) const
{
	return edge_orientation2d_function<Curve >(this, iCombination);
}

Vec3r Curve::orientation3d(point_iterator it) const
{
	return (*it)->orientation3d();
}

Vec3r Curve::orientation3d(int iCombination) const
{
	return edge_orientation3d_function<Curve >(this, iCombination);
}

real curvature2d(point_iterator it) const
{
	return (*it)->curvature2d();
}

real curvature2d(int iCombination = 0) const;

Material Curve::material() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	const Material& mat = (*v)->material();
	for (; v != vend; ++v) {
		if ((*v)->material() != mat)
			Exception::raiseException();
	}
	return mat;
}

int Curve::qi() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	int qi_ = (*v)->qi();
	for (; v != vend; ++v) {
		if ((*v)->qi() != qi_)
			Exception::raiseException();
	}
	return qi_;
}

occluder_container::const_iterator occluders_begin() const
{
	return _FEdgeA->occluders().begin();
}

occluder_container::const_iterator occluders_end() const
{
	return _FEdgeA->occluders().end();
}

int Curve::occluders_size() const
{
	return qi();
}

bool Curve::occluders_empty() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	bool empty = (*v)->occluders_empty();
	for (; v != vend; ++v) {
		if ((*v)->occluders_empty() != empty)
			Exception::raiseException();
	}
	return empty;
}

const Polygon3r& occludee() const
{
	return *(_FEdgeA->aFace());
}

const SShape *Curve::occluded_shape() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	const SShape *sshape = (*v)->occluded_shape();
	for (; v != vend; ++v) {
		if ((*v)->occluded_shape() != sshape)
			Exception::raiseException();
	}
	return sshape;
}

const bool Curve::occludee_empty() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	bool empty = (*v)->occludee_empty();
	for (; v != vend; ++v) {
		if ((*v)->occludee_empty() != empty)
			Exception::raiseException();
	}
	return empty;
}
real Curve::z_discontinuity(int iCombination) const
{
	return z_discontinuity_edge_function<Curve>(this, iCombination);
}

int Curve::shape_id() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	Id id = (*v)->shape_id();
	for (; v != vend; ++v) {
		if ((*v)->shape_id() != id)
			Exception::raiseException();
	}
	return id.first;
}


const SShape *Curve::shape() const
{
	const_vertex_iterator v = vertices_begin(), vend = vertices_end();
	const SShape *sshape = (*v)->shape();
	for (; v != vend; ++v) {
		if ((*v)->shape() != sshape)
			Exception::raiseException();
	}
	return sshape;
}


occluder_container::const_iterator Curve::occluders_begin() const
{
	const_vertex_iterator v = vertices_begin();
	return (*v)->occluders_begin();
}


occluder_container::const_iterator Curve::occluders_end() const
{
	const_vertex_iterator v = vertices_end();
	return (*v)->occluders_end();
}

Vec3r Curve::curvature2d_as_vector(int iCombination) const
{
	return curvature2d_as_vector_edge_function<Curve>(this, iCombination);
}

real Curve::curvature2d_as_angle(int iCombination) const
{
	return curvature2d_as_angle_edge_function<Curve>(this, iCombination);
}

float Curve::shape_importance(int iCombination) const
{
	return shape_importance_edge_function<Curve>(this, iCombination);
}

float Curve::local_average_depth(int iCombination) const
{
	return local_average_depth_edge_function<Curve>(this, iCombination);
}

float Curve::local_depth_variance(int iCombination ) const
{
	return local_depth_variance_edge_function<Curve>(this, iCombination);
#if 0
	local_depth_variance_functor<Point> functor;
	float result;
	Evaluate<float, local_depth_variance_functor<Point> >(&functor, iCombination, result);
	return result;
#endif
}

real Curve::local_average_density(float sigma, int iCombination ) const
{
	return density_edge_function<Curve>(this, iCombination);
#if 0
	density_functor<Point> functor;
	real result;
	Evaluate<real, density_functor<Point> >(&functor, iCombination, result);
	return result;
#endif
}
#endif

/* UNUSED */
// #define EPS_CURVA_DIR 0.01

void Curve::computeCurvatureAndOrientation ()
{
#if 0
	const_vertex_iterator v = vertices_begin(), vend = vertices_end(), v2, prevV, v0;
	Vec2d p0, p1, p2;
	Vec3r p;

	p = (*v)->point2d();
	p0 = Vec2d(p[0], p[1]);
	prevV = v;
	++v;
	p = (*v)->point2d();
	p1 = Vec2d(p[0], p[1]);
	Vec2d prevDir(p1 - p0);

	for (; v! = vend; ++v) {
		v2 = v;
		++v2;
		if (v2 == vend)
			break;
		Vec3r p2 = (*v2)->point2d();

		Vec2d BA = p0 - p1;
		Vec2d BC = p2 - p1;
		real lba = BA.norm(), lbc = BC.norm();
		BA.normalizeSafe();
		BC.normalizeSafe();
		Vec2d normalCurvature = BA + BC;
		Vec2d dir = Vec2d(BC - BA);
		Vec2d normal = Vec2d(-dir[1], dir[0]);

		normal.normalizeSafe();
		real curvature = normalCurvature * normal;
		if (lba + lbc > MY_EPSILON)
			curvature /= (0.5 * lba + lbc);
		if (dir.norm() < MY_EPSILON)
			dir = 0.1 * prevDir;
		(*v)->setCurvatureFredo(curvature);
		(*v)->setDirectionFredo(dir);

		prevV = v;
		p0 = p1;
		p1 = p2;
		prevDir = dir;
		prevDir.normalize();
	}
	(*v)->setCurvatureFredo((*prevV)->curvatureFredo());
	(*v)->setDirectionFredo((*v)->point2d() - (*prevV)->point2d());
	v0 = vertices_begin();
	v2 = v0;
	++v2;
	(*v0)->setCurvatureFredo((*v2)->curvatureFredo());
	(*v0)->setDirectionFredo((*v2)->point2d() - (*v0)->point2d());

	//closed curve case one day...

	//
	return;

	//numerical degeneracy verification... we'll see later
	const_vertex_iterator vLastReliable = vertices_begin();

	v = vertices_begin();
	p = (*v)->point2d();
	p0 = Vec2d(p[0], p[1]);
	prevV = v;
	++v;
	p = (*v)->point2d();
	p1 = Vec2d(p[0], p[1]);
	bool isReliable = false;
	if ((p1 - p0).norm > EPS_CURVA) {
		vLastReliable = v;
		isReliable = true;
	}

	for (; v != vend; ++v) {
		v2 = v;
		++v2;
		if (v2 == vend)
			break;
		Vec3r p2 = (*v2)->point2d();

		Vec2d BA = p0 - p1;
		Vec2d BC = p2 - p1;
		real lba = BA.norm(), lbc = BC.norm();

		if ((lba + lbc) < EPS_CURVA) {
			isReliable = false;
			cerr << "/";
		}
		else {
			if (!isReliable) {  //previous points were not reliable
				const_vertex_iterator vfix = vLastReliable;
				++vfix;
				for (; vfix != v; ++vfix) {
					(*vfix)->setCurvatureFredo((*v)->curvatureFredo());
					(*vfix)->setDirectionFredo((*v)->directionFredo());
				}
			}
			isReliable = true;
			vLastReliable = v;
		}
		prevV = v;
		p0 = p1;
		p1 = p2;
	}
#endif
}

} /* namespace Freestyle */
