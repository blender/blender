/**
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

#include "BSP_GhostTest3D.h"

#include "BSP_TMesh.h"
#include "MEM_SmartPtr.h"
#include "BSP_PlyLoader.h"

#include <iostream>

using namespace std;
#if 1
	MEM_SmartPtr<BSP_TMesh> 
NewTestMesh(
	int x,
	int y,
	MT_Scalar fx,
	MT_Scalar fy,
	MT_Scalar ampx,
	MT_Scalar ampy,
	MT_Scalar sx,
	MT_Scalar sy
) {

	MEM_SmartPtr<BSP_TMesh> output = new BSP_TMesh;

	std::vector<BSP_TVertex> &verts = output->VertexSet();

	int i,j;

	MT_Scalar x_scale = fx*MT_PI/x;
	MT_Scalar y_scale = fy*MT_PI/y;

	MT_Scalar fsx = sx/x;
	MT_Scalar fsy = sy/y;

	for (j = 0; j < y; j++) {
		for (i = 0; i < x; i++) {
			float z = ampx*sin(x_scale * i) + ampy*sin(y_scale * j);

			MT_Vector3 val(i*fsx - sx/2,j*fsy - sy/2,z);

			BSP_TVertex chuff;
			chuff.m_pos = val;
			verts.push_back(chuff);
		}
	}

	int poly[4];

	for (j = 0; j < (y-1); j++) {
		for (i = 0; i < (x-1); i++) {

			poly[0] = j*x + i;
			poly[1] = poly[0] + 1;
			poly[2] = poly[1] + y;
			poly[3] = poly[2] -1;

			output->AddFace(poly,4);
		}
	}

	output->m_min = MT_Vector3(-sx/2,-sy/2,-ampx -ampy);
	output->m_max = MT_Vector3(sx/2,sy/2,ampx + ampy);

	return output;
}
#endif


int main()
{
	MT_Vector3 min,max;
	MT_Vector3 min2,max2;

#if 1
	MEM_SmartPtr<BSP_TMesh> mesh1 = BSP_PlyLoader::NewMeshFromFile("bsp_cube.ply",min,max);
	MEM_SmartPtr<BSP_TMesh> mesh2 = BSP_PlyLoader::NewMeshFromFile("bsp_cube.ply",min2,max2);

	mesh1->m_min = min;
	mesh1->m_max = max;
	mesh2->m_min = min2;
	mesh1->m_max = max2;

#else
	MEM_SmartPtr<BSP_TMesh> mesh1 = NewTestMesh(10,10,2,2,4,4,20,20);
	MEM_SmartPtr<BSP_TMesh> mesh2 = NewTestMesh(10,10,2,2,4,4,20,20);
#endif	

	if (!mesh1) {
		cout << "could not load mesh!";
		return 0;
	}



//	MEM_SmartPtr<BSP_TMesh> mesh2 = new BSP_TMesh(mesh1.Ref());

	BSP_GhostTestApp3D app;

	cout << "Mesh polygons :" << mesh1->FaceSet().size() << "\n";
	cout << "Mesh vertices :" << mesh1->VertexSet().size() << "\n";

	app.SetMesh(mesh1);
	app.SetMesh(mesh2);


	app.InitApp();
	
	app.Run();

    return 0;

}




