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

#include "CSG_IndexDefs.h"
#include <vector>  // Include STL vector class.
class MT_Plane3;

template <typename PGBinder> class CSG_Triangulate
{
public:
	
	CSG_Triangulate(
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
		const PGBinder& contour,
		const MT_Plane3 &normal,
		VIndexList &result
	);
	
	~CSG_Triangulate(
	);

private:

	// compute area of a contour/polygon
		MT_Scalar 
	Area(
		const PGBinder& contour
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


		bool 
	Snip(
		const PGBinder& contour,
		int u,
		int v,
		int w,
		int n,
		int *V
	);

	int m_xi;
	int m_yi;
	int m_zi;

	// Temporary storage

	VIndexList m_V;

};

#include "CSG_Triangulate.inl"

#endif


