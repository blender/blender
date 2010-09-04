/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "RAS_TexMatrix.h"

void RAS_CalcTexMatrix(RAS_TexVert p[3],MT_Point3& origin,MT_Vector3& udir,MT_Vector3& vdir)
{
// precondition: 3 vertices are non-colinear

	MT_Vector3 vec1 = p[1].xyz()-p[0].xyz();
	MT_Vector3 vec2 = p[2].xyz()-p[0].xyz();
	MT_Vector3 normal = vec1.cross(vec2);
	normal.normalize();

	// determine which coordinate we drop, ie. max coordinate in the normal
	

	int ZCOORD = normal.closestAxis();
	int XCOORD = (ZCOORD+1)%3;
	int YCOORD = (ZCOORD+2)%3;
		
	// ax+by+cz+d=0
	MT_Scalar d = -p[0].xyz().dot(normal);
	

	MT_Matrix3x3 mat3(	p[0].getUV1()[0],p[0].getUV1()[1],	1,
						p[1].getUV1()[0],p[1].getUV1()[1],	1,
						p[2].getUV1()[0],p[2].getUV1()[1],	1);


	MT_Matrix3x3 mat3inv = mat3.inverse();

	MT_Vector3 p123x(p[0].xyz()[XCOORD],p[1].xyz()[XCOORD],p[2].xyz()[XCOORD]);
	MT_Vector3 resultx = mat3inv*p123x;
	MT_Vector3 p123y(p[0].xyz()[YCOORD],p[1].xyz()[YCOORD],p[2].xyz()[YCOORD]);
	MT_Vector3 resulty = mat3inv*p123y;

	// normal[ZCOORD] is not zero, because it's chosen to be maximal (absolute), and normal has length 1, 
	// so at least on of the coords is <> 0

	//droppedvalue udir.dot(normal) =0
	MT_Scalar droppedu = -(resultx.x()*normal[XCOORD]+resulty.x()*normal[YCOORD])/normal[ZCOORD];
	udir[XCOORD] = resultx.x();
	udir[YCOORD] = resulty.x();
	udir[ZCOORD] = droppedu;
	MT_Scalar droppedv = -(resultx.y()*normal[XCOORD]+resulty.y()*normal[YCOORD])/normal[ZCOORD];
	vdir[XCOORD] = resultx.y();
	vdir[YCOORD] = resulty.y();
	vdir[ZCOORD] = droppedv;
	// droppedvalue b = -(ax+cz+d)/y;
	MT_Scalar droppedvalue = -((resultx.z()*normal[XCOORD] + resulty.z()*normal[YCOORD]+d))/normal[ZCOORD];
	origin[XCOORD] = resultx.z();
	origin[YCOORD] = resulty.z();
	origin[ZCOORD] = droppedvalue;
	

}

#ifdef _TEXOWNMAIN

int main()
{

	MT_Point2 puv0={0,0};
	MT_Point3 pxyz0 (0,0,128);

	MT_Scalar puv1[2]={1,0};
	MT_Point3 pxyz1(128,0,128);

	MT_Scalar puv2[2]={1,1};
	MT_Point3 pxyz2(128,0,0);

	RAS_TexVert p0(pxyz0,puv0);
	RAS_TexVert p1(pxyz1,puv1);
	RAS_TexVert p2(pxyz2,puv2);

	RAS_TexVert vertices[3] = 
	{
		p0,
		p1,
		p2
	};

	MT_Vector3 udir,vdir;
	MT_Point3 origin;
	CalcTexMatrix(vertices,origin,udir,vdir);

	MT_Point3 testpoint(128,32,64);

	MT_Scalar lenu = udir.length2();
	MT_Scalar lenv = vdir.length2();

	MT_Scalar testu=((pxyz2-origin).dot(udir))/lenu;
	MT_Scalar testv=((pxyz2-origin).dot(vdir))/lenv;




	return 0;
}

#endif // _TEXOWNMAIN
