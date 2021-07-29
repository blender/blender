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

#ifndef __POLYGON_H__
#define __POLYGON_H__

/** \file blender/freestyle/intern/geometry/Polygon.h
 *  \ingroup freestyle
 *  \brief Class to define a polygon
 *  \author Stephane Grabli
 *  \date 30/07/2002
 */

#include <vector>

#include "Geom.h"
#include "GeomUtils.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

namespace Geometry {

template <class Point>
class Polygon
{
public:
	inline Polygon()
	{
		_id = 0;
		userdata = 0;
		userdata2 = 0;
	}

	inline Polygon(const vector<Point>& vertices)
	{
		_vertices = vertices;
		computeBBox();
		_id = 0;
		userdata = 0;
		userdata2 = 0;
	}

	inline Polygon(const Polygon<Point>& poly)
	{
		Point p;
		for (typename vector<Point>::const_iterator it = poly.getVertices().begin();
		     it != poly.getVertices().end();
		     it++)
		{
			p = *it;
			_vertices.push_back(p);
		}

		_id = poly.getId();
		poly.getBBox(_min, _max);
		userdata = 0;
		userdata2 = 0;
	}

	virtual ~Polygon() {}

	//
	// Accessors
	//
	/////////////////////////////////////////////////////////////////////////////
	inline const vector<Point>& getVertices() const
	{
		return _vertices;
	}

	inline void getBBox(Point& min, Point& max) const
	{
		min = _min;
		max = _max;
	}

	inline Point getBBoxCenter()
	{
		Point result;
		result = (_min + _max) / 2;
		return result;
	}

	inline Point getCenter()
	{
		Point result;
		for (typename vector<Point>::iterator it = _vertices.begin(); it != _vertices.end(); it++)
			result += *it;
		result /= _vertices.size();
		return result;
	}

	inline unsigned getId() const
	{
		return _id;
	}

	//
	// Modifiers
	//
	/////////////////////////////////////////////////////////////////////////////
	inline void setVertices(const vector<Point>& vertices)
	{
		_vertices.clear();
		Point p;
		for (typename vector<Point>::const_iterator it = vertices.begin(); it != vertices.end(); it++) {
			p = *it;
			_vertices.push_back(p);
		}
		computeBBox();
	}

	inline void setId(unsigned id)
	{
		_id = id;
	}

	//
	// Other methods
	//
	/////////////////////////////////////////////////////////////////////////////
	inline void computeBBox()
	{
		if (_vertices.empty())
			return;

		_max = _vertices[0];
		_min = _vertices[0];

		for (typename vector<Point>::iterator it = _vertices.begin(); it != _vertices.end(); it++) {
			for (unsigned int i = 0; i < Point::dim(); i++) {
				if ((*it)[i] > _max[i])
					_max[i] = (*it)[i];
				if ((*it)[i] < _min[i])
					_min[i] = (*it)[i];
			}
		}
	}

	// FIXME Is it possible to get rid of userdatas ?
	void *userdata;
	void *userdata2; // Used during ray casting

protected:
	vector<Point> _vertices;
	Point _min;
	Point _max;
	unsigned _id;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Geometry:Polygon")
#endif
};


//
// Polygon3r class
//
///////////////////////////////////////////////////////////////////////////////
class Polygon3r : public Polygon<Vec3r>
{
public:
	inline Polygon3r() : Polygon<Vec3r>() {}

	inline Polygon3r(const vector<Vec3r>& vertices, const Vec3r& normal) : Polygon<Vec3r>(vertices)
	{
		setNormal(normal);
	}

	inline Polygon3r(const Polygon3r& poly) : Polygon<Vec3r>(poly), _normal(poly._normal) {}

	virtual ~Polygon3r() {}

	void setNormal(const Vec3r& normal)
	{
		_normal = normal;
	}

	inline Vec3r getNormal() const
	{
		return _normal;
	}

	/*! Check whether the Polygon intersects with the ray or not */
	inline bool rayIntersect(const Vec3r& orig, const Vec3r& dir, real& t, real& u, real& v,
	                         real epsilon = M_EPSILON) const
	{
#if 0
	if (_vertices.size() < 3)
		return false;
#endif
		return GeomUtils::intersectRayTriangle(orig, dir, _vertices[0], _vertices[1], _vertices[2], t, u, v, epsilon);
	}

private:
	Vec3r _normal;
};

} // end of namespace Geometry

} /* namespace Freestyle */

#endif // __POLYGON_H__
