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

#include <stdio.h>

#include <stdlib.h>
#include "MT_Plane3.h"
#include "BSP_Triangulate.h"
#include "MT_assert.h"

static const MT_Scalar EPSILON = MT_Scalar(1e-10);

using namespace std;

BSP_Triangulate::
BSP_Triangulate(
):
	m_xi(0),
	m_yi(1)
{
}

BSP_Triangulate::
~BSP_Triangulate(
){
}


	MT_Scalar 
BSP_Triangulate::
Area(
	const vector<BSP_MVertex> &verts,
	const BSP_VertexList &contour
){

  int n = contour.size();

  MT_Scalar A(0.0);

  for(int p=n-1,q=0; q<n; p=q++)
  {
    A+= verts[contour[p]].m_pos[m_xi]*verts[contour[q]].m_pos[m_yi] - 
		verts[contour[q]].m_pos[m_xi]*verts[contour[p]].m_pos[m_yi];
  }
  return A*MT_Scalar(0.5);
}

/*
 InsideTriangle decides if a point P is Inside of the triangle
 defined by A, B, C.
 Or within an epsilon of it.
*/

	bool 
BSP_Triangulate::
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

	bool 
BSP_Triangulate::
Snip(
	const vector<BSP_MVertex> &verts,
	const BSP_VertexList &contour,
	int u,int v,
	int w,int n,
	int *V
){
  int p;
  MT_Scalar Ax, Ay, Bx, By, Cx, Cy, Px, Py;

  Ax = verts[contour[V[u]]].m_pos[m_xi];
  Ay = verts[contour[V[u]]].m_pos[m_yi];

  Bx = verts[contour[V[v]]].m_pos[m_xi];
  By = verts[contour[V[v]]].m_pos[m_yi];

  Cx = verts[contour[V[w]]].m_pos[m_xi];
  Cy = verts[contour[V[w]]].m_pos[m_yi];

  // Snip is passes if the area of the candidate triangle is
  // greater  than 2*epsilon
  // And if none of the remaining vertices are inside the polygon
  // or within an epsilon of the boundary,

  if ( EPSILON > (((Bx-Ax)*(Cy-Ay)) - ((By-Ay)*(Cx-Ax))) ) return false;

  for (p=0;p<n;p++)
  {
    if( (p == u) || (p == v) || (p == w) ) continue;
    Px = verts[contour[V[p]]].m_pos[m_xi];
    Py = verts[contour[V[p]]].m_pos[m_yi];
    if (InsideTriangle(Ax,Ay,Bx,By,Cx,Cy,Px,Py)) return false;
  }

  return true;
}

	bool 
BSP_Triangulate::
Process(
	const vector<BSP_MVertex> &verts,
	const BSP_VertexList &contour,
	const MT_Plane3 &normal,
	std::vector<int> &result
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

  /* initialize list of Vertices in polygon */

  int n = contour.size();
  if ( n < 3 ) return false;

  /* we want a counter-clockwise polygon in V */
  /* to true but we also nead to preserve the winding order
	 of polygons going into the routine. We keep track of what
	 we did with a little bool */
  bool is_flipped = false;

  if ( 0.0f < Area(verts,contour) ) {
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
#if 0
	  int deb = 0;
	  for (deb= 0; deb < contour.size(); deb++) {
		cout << verts[contour[deb]].m_pos << "\n";
	  }
	  cout.flush();	
#endif
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

    if ( Snip(verts,contour,u,v,w,nv, &m_V[0]) )
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


