/**
 * $Id$
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef TRIANGULATE_H


#define TRIANGULATE_H

/*****************************************************************/
/** Static class to triangulate any contour/polygon efficiently **/
/** You should replace Vector2d with whatever your own Vector   **/
/** class might be.  Does not support polygons with holes.      **/
/** Uses STL vectors to represent a dynamic array of vertices.  **/
/** This code snippet was submitted to FlipCode.com by          **/
/** John W. Ratcliff (jratcliff@verant.com) on July 22, 2000    **/
/** I did not write the original code/algorithm for this        **/
/** this triangulator, in fact, I can't even remember where I   **/
/** found it in the first place.  However, I did rework it into **/
/** the following black-box static class so you can make easy   **/
/** use of it in your own code.  Simply replace Vector2d with   **/
/** whatever your own Vector implementation might be.           **/
/*****************************************************************/


#include <vector>  // Include STL vector class.
#include "MT_Point3.h"
#include "BSP_MeshPrimitives.h"

class MT_Plane3;

class BSP_Triangulate
{
public:

	BSP_Triangulate(
	);

	// triangulate a contour/polygon, places results in STL vector
	// as series of triangles. IT uses the major axis of the normal
	// to turn it into a 2d problem.

	// Should chaange this to accept a point array and a list of 
	// indices into that point array. Result should be indices of those
	// indices.
	//
	// MT_Point3 global_array
	// vector<BSP_VertexInd> polygon
	// result is vector<int> into polygon.

		bool
	Process(
		const std::vector<BSP_MVertex> &verts,
		const BSP_VertexList &contour,
		const MT_Plane3 &normal,
		std::vector<int> &result
	);
	
	// compute area of a contour/polygon
		MT_Scalar 
	Area(
		const std::vector<BSP_MVertex> &verts,
		const BSP_VertexList &contour
	);

	// decide if point Px/Py is inside triangle defined by
	// (Ax,Ay) (Bx,By) (Cx,Cy)

		bool 
	InsideTriangle(
		MT_Scalar Ax, MT_Scalar Ay,
		MT_Scalar Bx, MT_Scalar By,
		MT_Scalar Cx, MT_Scalar Cy,
		MT_Scalar Px, MT_Scalar Py
	);

	~BSP_Triangulate(
	);

private:

		bool 
	Snip(
		const std::vector<BSP_MVertex> &verts,
		const BSP_VertexList &contour,
		int u,
		int v,
		int w,
		int n,
		int *V
	);

	int m_xi;
	int m_yi;

	// Temporary storage

	std::vector<int> m_V;

};


#endif


