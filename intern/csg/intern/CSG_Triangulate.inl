/*
  CSGLib - Software Library for Constructive Solid Geometry
  Copyright (C) 2003-2004  Laurence Bourn

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Please send remarks, questions and bug reports to laurencebourn@hotmail.com
*/
#include "MT_Plane3.h"
#include "MT_Point3.h"

static const MT_Scalar EPSILON = MT_Scalar(1e-10);

template <typename PGBinder> 
CSG_Triangulate<PGBinder>::
CSG_Triangulate(
):
	m_xi(0),
	m_yi(1)
{
}

template <typename PGBinder> 
CSG_Triangulate<PGBinder>::
~CSG_Triangulate(
){
}


template <typename PGBinder> 
	MT_Scalar 
CSG_Triangulate<PGBinder>::
Area(
	const PGBinder& contour
){

  int n = contour.Size();
  MT_Scalar A(0.0);

  for(int p=n-1,q=0; q<n; p=q++)
  {
	A+= contour[p][m_xi]*contour[q][m_yi] - 
		contour[q][m_xi]*contour[p][m_yi];
  }
  return A*MT_Scalar(0.5);
}

/*
 InsideTriangle decides if a point P is Inside of the triangle
 defined by A, B, C.
 Or within an epsilon of it.
*/

template <typename PGBinder> 
	bool 
CSG_Triangulate<PGBinder>::
InsideTriangle(
	MT_Scalar Ax, MT_Scalar Ay,
    MT_Scalar Bx, MT_Scalar By,
    MT_Scalar Cx, MT_Scalar Cy,
    MT_Scalar Px, MT_Scalar Py
){
  MT_Scalar ax, ay, bx, by, cx, cy, apx, apy, bpx, bpy, cpx, cpy;
  MT_Scalar cCROSSap, bCROSScp, aCROSSbp;

  ax = Cx - Bx;  ay = Cy - By;
  bx = Ax - Cx;  by = Ay - Cy;
  cx = Bx - Ax;  cy = By - Ay;
  apx= Px - Ax;  apy= Py - Ay;
  bpx= Px - Bx;  bpy= Py - By;
  cpx= Px - Cx;  cpy= Py - Cy;

  aCROSSbp = ax*bpy - ay*bpx;
  cCROSSap = cx*apy - cy*apx;
  bCROSScp = bx*cpy - by*cpx;

  return ((aCROSSbp >= -EPSILON) && (bCROSScp >= -EPSILON) && (cCROSSap >= -EPSILON));
};

template <typename PGBinder> 
	bool 
CSG_Triangulate<PGBinder>::
Snip(
	const PGBinder& contour,
	int u,int v,
	int w,int n,
	int *V
){
  MT_Scalar Ax, Ay, Bx, By, Cx, Cy;

  Ax = contour[V[u]][m_xi];
  Ay = contour[V[u]][m_yi];

  Bx = contour[V[v]][m_xi];
  By = contour[V[v]][m_yi];

  Cx = contour[V[w]][m_xi];
  Cy = contour[V[w]][m_yi];

  // Snip is passes if the area of the candidate triangle is
  // greater  than 2*epsilon
  // And if none of the remaining vertices are inside the polygon
  // or within an epsilon of the boundary,

  if ( EPSILON > (((Bx-Ax)*(Cy-Ay)) - ((By-Ay)*(Cx-Ax))) ) return false;
#if 1
  // this check is only needed for non-convex polygons 
  // well yeah but convex to me and you is not necessarily convex to opengl.
  int p;
  MT_Scalar Px,Py;	

  for (p=0;p<n;p++)
  {
    if( (p == u) || (p == v) || (p == w) ) continue;
    Px = contour[V[p]][m_xi];
    Py = contour[V[p]][m_yi];
    if (InsideTriangle(Ax,Ay,Bx,By,Cx,Cy,Px,Py)) return false;
  }
#endif
  return true;
}

template <typename PGBinder> 
	bool 
CSG_Triangulate<PGBinder>::
Process(
	const PGBinder& contour,
	const MT_Plane3 &normal,
	VIndexList &result
){

	// Choose major axis of normal and assign 
	// 'projection' indices m_xi,m_yi;

	int maj_axis = normal.Normal().closestAxis();

	if (maj_axis == 0) {
		m_xi = 1; m_yi = 2;
	} else 
	if (maj_axis == 1) {
		m_xi = 0; m_yi = 2;
	} else {
		m_xi = 0; m_yi = 1;
	}
	m_zi = maj_axis;

  /* initialize list of Vertices in polygon */

  int n = contour.Size();
  if ( n < 3 ) return false;

  /* we want a counter-clockwise polygon in V */
  /* to true but we also nead to preserve the winding order
	 of polygons going into the routine. We keep track of what
	 we did with a little bool */
  bool is_flipped = false;

  if ( 0.0f < Area(contour) ) {
    for (int v=0; v<n; v++) m_V.push_back(v);
  } else {
    for(int v=0; v<n; v++) m_V.push_back((n-1)-v);
	is_flipped = true;
  }

  int nv = n;

  /*  remove nv-2 Vertices, creating 1 triangle every time */
  int count = 2*nv;   /* error detection */

  for(int m=0, v=nv-1; nv>2; )
  {
    /* if we loop, it is probably a non-simple polygon */
    if (0 >= (count--))
    {
      //** Triangulate: ERROR - probable bad polygon!
	  m_V.clear();
      return false;
    }

    /* three consecutive vertices in current polygon, <u,v,w> */
    int u = v  ; if (nv <= u) u = 0;     /* previous */
    v = u+1; if (nv <= v) v = 0;     /* new v    */
    int w = v+1; if (nv <= w) w = 0;     /* next     */

	/* Try and snip this triangle off from the 
	   current polygon.*/

    if ( Snip(contour,u,v,w,nv,m_V.begin()) )
    {
      int a,b,c,s,t;

      /* true names of the vertices */
      a = m_V[u]; b = m_V[v]; c = m_V[w];

      /* output Triangle indices*/
	  if (is_flipped) {
		  result.push_back( c );
		  result.push_back( b );
		  result.push_back( a );
	  } else {
		  result.push_back( a );
		  result.push_back( b );
		  result.push_back( c );
	  }

      m++;

      /* remove v from remaining polygon */
      for(s=v,t=v+1;t<nv;s++,t++) m_V[s] = m_V[t]; nv--;

      /* resest error detection counter */
      count = 2*nv;
    }
  }

  m_V.clear();
  return true;
}


