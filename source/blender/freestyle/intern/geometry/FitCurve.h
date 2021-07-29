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

#ifndef __FITCURVE_H__
#define __FITCURVE_H__

/** \file blender/freestyle/intern/geometry/FitCurve.h
 *  \ingroup freestyle
 *  \brief An Algorithm for Automatically Fitting Digitized Curves by Philip J. Schneider,
 *  \brief from "Graphics Gems", Academic Press, 1990
 *  \author Stephane Grabli
 *  \date 06/06/2003
 */

#include <vector>

#include "Geom.h"

#include "../system/FreestyleConfig.h"

namespace Freestyle {

using namespace Geometry;

/* 2d point */
typedef struct Point2Struct
{
	double coordinates[2];

	Point2Struct()
	{
		coordinates[0] = 0;
		coordinates[1] = 0;
	}

	inline double operator[](const int i) const
	{
		return coordinates[i];
	}

	inline double& operator[](const int i)
	{
		return coordinates[i];
	}

	inline double x() const
	{
		return coordinates[0];
	}

	inline double y() const
	{
		return coordinates[1];
	}
} Point2;

typedef Point2 Vector2;


class FitCurveWrapper
{
private:
	std::vector<Vector2> _vertices;

public:
	FitCurveWrapper();
	~FitCurveWrapper();

	/*! Fits a set of 2D data points to a set of Bezier Curve segments
	 *    data
	 *      Input data points
	 *    oCurve
	 *      Control points of the sets of bezier curve segments.
	 *      Each segment is made of 4 points (polynomial degree of curve = 3)
	 *    error
	 *      max error tolerance between resulting curve and input data 
	 */
	void FitCurve(std::vector<Vec2d>& data, std::vector<Vec2d>& oCurve, double error);

protected:
	/* Vec2d  *d;    Array of digitized points
	 * int    nPts;  Number of digitized points
	 * double error; User-defined error squared
	 */
	void FitCurve(Vector2 *d, int nPts, double error);

	/*! Draws a Bezier curve segment
	 *  n
	 *    degree of curve (=3)
	 *  curve
	 *    bezier segments control points
	 */
	void DrawBezierCurve(int n, Vector2 *curve);

	/* Vec2d  *d;           Array of digitized points
	 * int    first, last;  Indices of first and last pts in region
	 * Vec2d  tHat1, tHat2; Unit tangent vectors at endpoints
	 * double error;        User-defined error squared
	 */
	void FitCubic(Vector2 *d, int first, int last, Vector2 tHat1, Vector2 tHat2, double error);
};

} /* namespace Freestyle */

#endif // __FITCURVE_H__
