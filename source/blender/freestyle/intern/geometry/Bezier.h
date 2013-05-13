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

#ifndef __BEZIER_H__
#define __BEZIER_H__

/** \file blender/freestyle/intern/geometry/Bezier.h
 *  \ingroup freestyle
 *  \brief Class to define a Bezier curve of order 4.
 *  \author Stephane Grabli
 *  \date 04/06/2003
 */

#include <vector>

#include "Geom.h"

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

using namespace Geometry;

class LIB_GEOMETRY_EXPORT BezierCurveSegment
{
private:
	std::vector<Vec2d> _ControlPolygon;
	std::vector<Vec2d> _Vertices;

public:
	BezierCurveSegment();
	virtual ~BezierCurveSegment();

	void AddControlPoint(const Vec2d& iPoint);
	void Build();

	inline int size() const
	{
		return _ControlPolygon.size();
	}

	inline std::vector<Vec2d>& vertices()
	{
		return _Vertices;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BezierCurveSegment")
#endif
};

class LIB_GEOMETRY_EXPORT BezierCurve 
{
private:
	std::vector<Vec2d> _ControlPolygon;
	std::vector<BezierCurveSegment*> _Segments;
	BezierCurveSegment *_currentSegment;

public:
	BezierCurve();
	BezierCurve(std::vector<Vec2d>& iPoints, double error=4.0);
	virtual ~BezierCurve();

	void AddControlPoint(const Vec2d& iPoint);

	std::vector<Vec2d>& controlPolygon()
	{
		return _ControlPolygon;
	}

	std::vector<BezierCurveSegment*>& segments()
	{
		return _Segments;
	}

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:BezierCurve")
#endif
};

} /* namespace Freestyle */

#endif // __BEZIER_H__
