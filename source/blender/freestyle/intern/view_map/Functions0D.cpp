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

/** \file blender/freestyle/intern/view_map/Functions0D.cpp
 *  \ingroup freestyle
 *  \brief Functions taking 0D input
 *  \author Stephane Grabli
 *  \author Emmanuel Turquin
 *  \date 01/07/2003
 */

#include "Functions0D.h"
#include "ViewMap.h"

#include "BKE_global.h"

using namespace std;

namespace Freestyle {

namespace Functions0D {

// Internal function
FEdge *getFEdge(Interface0D& it1, Interface0D& it2)
{
	return it1.getFEdge(it2);
}

void getFEdges(Interface0DIterator& it, FEdge *&fe1, FEdge *&fe2)
{
	// count number of vertices
	Interface0DIterator prev = it, next = it;
	++next;
	int count = 1;
	if (!it.isBegin() && !next.isEnd()) {
		count = 3;
	}
	if (count < 3) {
		// if we only have 2 vertices
		FEdge *fe = 0;
		Interface0DIterator tmp = it;
		if (it.isBegin()) {
			++tmp;
			fe = it->getFEdge(*tmp);
		}
		else {
			--tmp;
			fe = it->getFEdge(*tmp);
		}
		fe1 = fe;
		fe2 = NULL;
	}
	else {
		// we have more than 2 vertices
		bool begin = false, last = false;
		Interface0DIterator previous = it;
		if (!previous.isBegin())
			--previous;
		else
			begin = true;
		Interface0DIterator next = it;
		++next;
		if (next.isEnd())
			last = true;
		if (begin) {
			fe1 = it->getFEdge(*next);
			fe2 = NULL;
		}
		else if (last) {
			fe1 = previous->getFEdge(*it);
			fe2 = NULL;
		}
		else {
			fe1 = previous->getFEdge(*it);
			fe2 = it->getFEdge(*next);
		}
	}
}

void getViewEdges(Interface0DIterator &it, ViewEdge *&ve1, ViewEdge *&ve2)
{
	FEdge *fe1, *fe2;
	getFEdges(it, fe1, fe2);
	ve1 = fe1->viewedge();
	if (fe2 != NULL) {
		ve2 = fe2->viewedge();
		if (ve2 == ve1)
			ve2 = NULL;
	}
	else {
		ve2 = NULL;
	}
}

ViewShape *getShapeF0D(Interface0DIterator& it)
{
	ViewEdge *ve1, *ve2;
	getViewEdges(it, ve1, ve2);
	return ve1->viewShape();
}

void getOccludersF0D(Interface0DIterator& it, set<ViewShape*>& oOccluders)
{
	ViewEdge *ve1, *ve2;
	getViewEdges(it, ve1, ve2);
	occluder_container::const_iterator oit = ve1->occluders_begin();
	occluder_container::const_iterator oitend = ve1->occluders_end();

	for (; oit != oitend; ++oit)
		oOccluders.insert((*oit));

	if (ve2 != NULL) {
		oit = ve2->occluders_begin();
		oitend = ve2->occluders_end();
		for (; oit != oitend; ++oit)
			oOccluders.insert((*oit));
	}
}

ViewShape *getOccludeeF0D(Interface0DIterator& it)
{
	ViewEdge *ve1, *ve2;
	getViewEdges(it, ve1, ve2);
	ViewShape *aShape = ve1->aShape();
	return aShape;
}

//
int VertexOrientation2DF0D::operator()(Interface0DIterator& iter)
{
	Vec2f A, C;
	Vec2f B(iter->getProjectedX(), iter->getProjectedY());
	if (iter.isBegin()) {
		A = Vec2f(iter->getProjectedX(), iter->getProjectedY());
	}
	else {
		Interface0DIterator previous = iter;
		--previous ;
		A = Vec2f(previous->getProjectedX(), previous->getProjectedY());
	}
	Interface0DIterator next = iter;
	++next;
	if (next.isEnd())
		C = Vec2f(iter->getProjectedX(), iter->getProjectedY());
	else
		C = Vec2f(next->getProjectedX(), next->getProjectedY());

	Vec2f AB(B - A);
	if (AB.norm() != 0)
		AB.normalize();
	Vec2f BC(C - B);
	if (BC.norm() != 0)
		BC.normalize();
	result = AB + BC;
	if (result.norm() != 0)
		result.normalize();
	return 0;
}

int VertexOrientation3DF0D::operator()(Interface0DIterator& iter)
{
	Vec3r A, C;
	Vec3r B(iter->getX(), iter->getY(), iter->getZ());
	if (iter.isBegin()) {
		A = Vec3r(iter->getX(), iter->getY(), iter->getZ());
	}
	else {
		Interface0DIterator previous = iter;
		--previous ;
		A = Vec3r(previous->getX(), previous->getY(), previous->getZ());
	}
	Interface0DIterator next = iter;
	++next ;
	if (next.isEnd())
		C = Vec3r(iter->getX(), iter->getY(), iter->getZ());
	else
		C = Vec3r(next->getX(), next->getY(), next->getZ());

	Vec3r AB(B - A);
	if (AB.norm() != 0)
		AB.normalize();
	Vec3r BC(C - B);
	if (BC.norm() != 0)
		BC.normalize();
	result = AB + BC;
	if (result.norm() != 0)
		result.normalize();
	return 0;
}

int Curvature2DAngleF0D::operator()(Interface0DIterator& iter)
{
	Interface0DIterator tmp1 = iter, tmp2 = iter;
	++tmp2;
	unsigned count = 1;
	while ((!tmp1.isBegin()) && (count < 3)) {
		--tmp1;
		++count;
	}
	while ((!tmp2.isEnd()) && (count < 3)) {
		++tmp2;
		++count;
	}
	if (count < 3) {
		// if we only have 2 vertices
		result = 0;
		return 0;
	}

	Interface0DIterator v = iter;
	if (iter.isBegin())
		++v;
	Interface0DIterator next = v;
	++next;
	if (next.isEnd()) {
		next = v;
		--v;
	}
	Interface0DIterator prev = v;
	--prev;

	Vec2r A(prev->getProjectedX(), prev->getProjectedY());
	Vec2r B(v->getProjectedX(), v->getProjectedY());
	Vec2r C(next->getProjectedX(), next->getProjectedY());
	Vec2r AB(B - A);
	Vec2r BC(C - B);
	Vec2r N1(-AB[1], AB[0]);
	if (N1.norm() != 0)
		N1.normalize();
	Vec2r N2(-BC[1], BC[0]);
	if (N2.norm() != 0)
		N2.normalize();
	if ((N1.norm() == 0) && (N2.norm() == 0)) {
		Exception::raiseException();
		result = 0;
		return -1; 
	}
	double cosin = N1 * N2;
	if (cosin > 1)
		cosin = 1;
	if (cosin < -1)
		cosin = -1;
	result = acos(cosin);
	return 0;
}

int ZDiscontinuityF0D::operator()(Interface0DIterator& iter)
{
	FEdge *fe1, *fe2;
	getFEdges(iter, fe1, fe2);
	result = fe1->z_discontinuity();
	if (fe2 != NULL) {
		result += fe2->z_discontinuity();
		result /= 2.0f;
	}
	return 0;
}

int Normal2DF0D::operator()(Interface0DIterator& iter)
{
	FEdge *fe1, *fe2;
	getFEdges(iter, fe1, fe2);
	Vec3f e1(fe1->orientation2d());
	Vec2f n1(e1[1], -e1[0]);
	Vec2f n(n1);
	if (fe2 != NULL) {
		Vec3f e2(fe2->orientation2d());
		Vec2f n2(e2[1], -e2[0]);
		n += n2;
	}
	n.normalize();
	result = n;
	return 0;
}

int MaterialF0D::operator()(Interface0DIterator& iter)
{
	FEdge *fe1, *fe2;
	getFEdges(iter, fe1, fe2);
	if (fe1 == NULL)
		return -1;
	if (fe1->isSmooth())
		result = ((FEdgeSmooth *)fe1)->frs_material();
	else
		result = ((FEdgeSharp *)fe1)->bFrsMaterial();
#if 0
	const SShape *sshape = getShapeF0D(iter);
	return sshape->material();
#endif
	return 0;
}

int ShapeIdF0D::operator()(Interface0DIterator& iter)
{
	ViewShape *vshape = getShapeF0D(iter);
	result = vshape->getId();
	return 0;
}

int QuantitativeInvisibilityF0D::operator()(Interface0DIterator& iter)
{
	ViewEdge *ve1, *ve2;
	getViewEdges(iter, ve1, ve2);
	unsigned int qi1, qi2;
	qi1 = ve1->qi();
	if (ve2 != NULL) {
		qi2 = ve2->qi();
		if (qi2 != qi1) {
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << "QuantitativeInvisibilityF0D: ambiguous evaluation for point " << iter->getId() << endl;
			}
		}
	}
	result = qi1;
	return 0;
}

int CurveNatureF0D::operator()(Interface0DIterator& iter)
{
	Nature::EdgeNature nat = 0;
	ViewEdge *ve1, *ve2;
	getViewEdges(iter, ve1, ve2);
	nat |= ve1->getNature();
	if (ve2 != NULL)
		nat |= ve2->getNature();
	result = nat;
	return 0;
}

int GetOccludersF0D::operator()(Interface0DIterator& iter)
{
	set<ViewShape*> occluders;
	getOccludersF0D(iter, occluders);
	result.clear();
	//vsOccluders.insert(vsOccluders.begin(), occluders.begin(), occluders.end());
	for (set<ViewShape*>::iterator it = occluders.begin(), itend = occluders.end(); it != itend; ++it) {
		result.push_back((*it));
	}
	return 0;
}

int GetShapeF0D::operator()(Interface0DIterator& iter)
{
	result = getShapeF0D(iter);
	return 0;
}

int GetOccludeeF0D::operator()(Interface0DIterator& iter)
{
	result = getOccludeeF0D(iter);
	return 0;
}

} // end of namespace Functions0D

} /* namespace Freestyle */
