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

#ifndef __GRIDHELPERS_H__
#define __GRIDHELPERS_H__

/** \file blender/freestyle/intern/geometry/GridHelpers.h
 *  \ingroup freestyle
 *  \brief Class to define a cell grid surrounding the projected image of a scene
 *  \author Alexander Beels
 *  \date 2010-12-13
 */

#include <vector>

#include "FRS_freestyle.h"

#include "GeomUtils.h"
#include "Polygon.h"

#include "../winged_edge/WEdge.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

namespace GridHelpers {

/*! Computes the distance from a point P to a segment AB */
template<class T>
T closestPointToSegment(const T& P, const T& A, const T& B, real& distance)
{
	T AB, AP, BP;
	AB = B - A;
	AP = P - A;
	BP = P - B;

	real c1(AB * AP);
	if (c1 <= 0) {
		distance = AP.norm();
		return A; // A is closest point
	}

	real c2(AB * AB);
	if (c2 <= c1) {
		distance = BP.norm();
		return B; // B is closest point
	}

	real b = c1 / c2;
	T Pb, PPb;
	Pb = A + b * AB;
	PPb = P - Pb;

	distance = PPb.norm();
	return Pb; // closest point lies on AB
} 

inline Vec3r closestPointOnPolygon(const Vec3r& point, const Polygon3r& poly)
{
	// First cast a ray from the point onto the polygon plane
	// If the ray intersects the polygon, then the intersection point
	// is the closest point on the polygon
	real t, u, v;
	if (poly.rayIntersect(point, poly.getNormal(), t, u, v)) {
		return point + poly.getNormal() * t;
	}

	// Otherwise, get the nearest point on each edge, and take the closest
	real distance;
	Vec3r closest = closestPointToSegment(point, poly.getVertices()[2], poly.getVertices()[0], distance);
	for (unsigned int i = 0; i < 2; ++i) {
		real t;
		Vec3r p = closestPointToSegment(point, poly.getVertices()[i], poly.getVertices()[i + 1], t);
		if (t < distance) {
			distance = t;
			closest = p;
		}
	}
	return closest;
}

inline real distancePointToPolygon(const Vec3r& point, const Polygon3r& poly)
{
	// First cast a ray from the point onto the polygon plane
	// If the ray intersects the polygon, then the intersection point
	// is the closest point on the polygon
	real t, u, v;
	if (poly.rayIntersect(point, poly.getNormal(), t, u, v)) {
		return (t > 0.0) ? t : -t;
	}

	// Otherwise, get the nearest point on each edge, and take the closest
	real distance = GeomUtils::distPointSegment(point, poly.getVertices()[2], poly.getVertices()[0]);
	for (unsigned int i = 0; i < 2; ++i) {
		real t = GeomUtils::distPointSegment(point, poly.getVertices()[i], poly.getVertices()[i + 1]);
		if (t < distance) {
			distance = t;
		}
	}
	return distance;
}

class Transform
{
public:
	virtual ~Transform () = 0;
	virtual Vec3r operator()(const Vec3r& point) const = 0;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:GridHelpers:Transform")
#endif
};

inline bool insideProscenium (const real proscenium[4], const Polygon3r& polygon)
{
	// N.B. The bounding box check is redundant for inserting occluders into cells, because the cell selection code
	// in insertOccluders has already guaranteed that the bounding boxes will overlap.
	// First check the viewport edges, since they are the easiest case
	// Check if the bounding box is entirely outside the proscenium
	Vec3r bbMin, bbMax;
	polygon.getBBox(bbMin, bbMax);
	if (bbMax[0] < proscenium[0] || bbMin[0] > proscenium[1] || bbMax[1] < proscenium[2] || bbMin[1] > proscenium[3]) {
		return false;
	}

	Vec3r boxCenter(proscenium[0] + (proscenium[1] - proscenium[0]) / 2.0,
	                proscenium[2] + (proscenium[3] - proscenium[2]) / 2.0, 0.0);
	Vec3r boxHalfSize((proscenium[1] - proscenium[0]) / 2.0,
	                  (proscenium[3] - proscenium[2]) / 2.0, 1.0);
	Vec3r triverts[3] = {
		Vec3r(polygon.getVertices()[0][0], polygon.getVertices()[0][1], 0.0),
		Vec3r(polygon.getVertices()[1][0], polygon.getVertices()[1][1], 0.0),
		Vec3r(polygon.getVertices()[2][0], polygon.getVertices()[2][1], 0.0)
	};
	return GeomUtils::overlapTriangleBox(boxCenter, boxHalfSize, triverts);
}

inline vector<Vec3r> enumerateVertices(const vector<WOEdge*>& fedges)
{
	vector<Vec3r> points;
	// Iterate over vertices, storing projections in points
	for (vector<WOEdge*>::const_iterator woe = fedges.begin(), woend = fedges.end(); woe != woend; woe++) {
		points.push_back((*woe)->GetaVertex()->GetVertex());
	}

	return points;
}

void getDefaultViewProscenium(real viewProscenium[4]);

inline void expandProscenium (real proscenium[4], const Polygon3r& polygon)
{
	Vec3r bbMin, bbMax;
	polygon.getBBox(bbMin, bbMax);

	const real epsilon = 1.0e-6;

	if (bbMin[0] <= proscenium[0]) {
		proscenium[0] = bbMin[0] - epsilon;
	}

	if (bbMin[1] <= proscenium[2]) {
		proscenium[2] = bbMin[1] - epsilon;
	}

	if (bbMax[0] >= proscenium[1]) {
		proscenium[1] = bbMax[0] + epsilon;
	}

	if (bbMax[1] >= proscenium[3]) {
		proscenium[3] = bbMax[1] + epsilon;
	}
}

inline void expandProscenium (real proscenium[4], const Vec3r& point)
{
	const real epsilon = 1.0e-6;

	if (point[0] <= proscenium[0]) {
		proscenium[0] = point[0] - epsilon;
	}

	if (point[1] <= proscenium[2]) {
		proscenium[2] = point[1] - epsilon;
	}

	if (point[0] >= proscenium[1]) {
		proscenium[1] = point[0] + epsilon;
	}

	if (point[1] >= proscenium[3]) {
		proscenium[3] = point[1] + epsilon;
	}
}

};  // GridHelpers namespace

} /* namespace Freestyle */

#endif // __GRIDHELPERS_H__
