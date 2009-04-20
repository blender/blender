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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

// implementation of external c api
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../extern/LOD_decimation.h"
#include "LOD_DecimationClass.h"

using namespace std;

	int 
LOD_LoadMesh(
	LOD_Decimation_InfoPtr info
) {
	if (info == NULL) return 0;
	if (
		info->vertex_buffer == NULL ||
		info->vertex_normal_buffer == NULL ||
		info->triangle_index_buffer == NULL
	) {
		return 0;
	}


	// create the intern object to hold all 
	// the decimation classes

	MEM_SmartPtr<LOD_DecimationClass> intern(LOD_DecimationClass::New(info));

	if (intern == NULL) return 0;

	MEM_SmartPtr<vector<LOD_Vertex> > intern_vertex_buffer(new vector<LOD_Vertex>(info->vertex_num));
	if (intern_vertex_buffer == NULL) return 0;

	vector<LOD_Vertex>::iterator intern_vertex_it(intern_vertex_buffer->begin());

	// now load in the vertices to the mesh

	const int vertex_stride = 3;

	float * vertex_ptr = info->vertex_buffer;
	const float * vertex_end = vertex_ptr + info->vertex_num*vertex_stride;
	
	LOD_ManMesh2 &mesh = intern->Mesh();

	for (;vertex_ptr < vertex_end; vertex_ptr += vertex_stride,++intern_vertex_it) {
		intern_vertex_it->pos = MT_Vector3(vertex_ptr);
	}
	
	mesh.SetVertices(intern_vertex_buffer);

	// load in the triangles
	
	const int triangle_stride = 3;

	int * triangle_ptr = info->triangle_index_buffer;
	const int * triangle_end = triangle_ptr + info->face_num*triangle_stride;

	try {

		for (;triangle_ptr < triangle_end; triangle_ptr += triangle_stride) {
			mesh.AddTriangle(triangle_ptr);
		}
	}

	catch (...) {
		return 0;
	}

	// ok we have built the mesh 

	intern->m_e_decimation_state = LOD_DecimationClass::e_loaded;

	info->intern = (void *) (intern.Release());	

	return 1;
}

	int 
LOD_PreprocessMesh(
	LOD_Decimation_InfoPtr info
) {
	if (info == NULL) return 0;
	if (info->intern == NULL) return 0;

	LOD_DecimationClass *intern = (LOD_DecimationClass *) info->intern;
	if (intern->m_e_decimation_state != LOD_DecimationClass::e_loaded) return 0;	

	// arm the various internal classes so that we are ready to step
	// through decimation

	intern->FaceEditor().BuildNormals();
	if (intern->Decimator().Arm() == false) return 0;

	// ok preprocessing done 
	intern->m_e_decimation_state = LOD_DecimationClass::e_preprocessed;

	return 1;
}

	int 
LOD_CollapseEdge(
	LOD_Decimation_InfoPtr info
){
	if (info == NULL) return 0;
	if (info->intern == NULL) return 0;
	LOD_DecimationClass *intern = (LOD_DecimationClass *) info->intern;
	if (intern->m_e_decimation_state != LOD_DecimationClass::e_preprocessed) return 0;	

	bool step_result = intern->Decimator().Step();

	return step_result == true ? 1 : 0;
}	

	
	int
LOD_FreeDecimationData(
	LOD_Decimation_InfoPtr info
){
	if (info == NULL) return 0;
	if (info->intern == NULL) return 0;
	LOD_DecimationClass *intern = (LOD_DecimationClass *) info->intern;
	delete(intern);
	info->intern = NULL;
	return 1;
}

