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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BSP_CSGMeshBuilder.h"


using namespace std;

	MEM_SmartPtr<BSP_CSGMesh> 
BSP_CSGMeshBuilder::
NewMesh(
	CSG_MeshPropertyDescriptor &props,
	CSG_FaceIteratorDescriptor &face_it,
	CSG_VertexIteratorDescriptor &vertex_it
) {
	
	MEM_SmartPtr<BSP_CSGMesh> mesh = BSP_CSGMesh::New();
	if (mesh == NULL) return NULL;

	MEM_SmartPtr<BSP_CSGUserData> fv_data = new BSP_CSGUserData(props.user_face_vertex_data_size);
	MEM_SmartPtr<BSP_CSGUserData> face_data = new BSP_CSGUserData(props.user_data_size);
	

	MEM_SmartPtr<vector<BSP_MVertex> > vertices(new vector<BSP_MVertex>);
	if (vertices == NULL || fv_data == NULL || face_data == NULL) return NULL;

	// The size of the vertex data array will be at least the number of faces.
 
	fv_data->Reserve(face_it.num_elements);
	face_data->Reserve(face_it.num_elements);

	vertices->reserve(vertex_it.num_elements);

	CSG_IVertex vertex;

	while (!vertex_it.Done(vertex_it.it)) {
		vertex_it.Fill(vertex_it.it,&vertex);

		MT_Point3 pos(vertex.position);
		vertices->push_back(BSP_MVertex(pos));

		vertex_it.Step(vertex_it.it);
	}
		
	// pass ownership of the vertices to the mesh.
	mesh->SetVertices(vertices);

	// now for the polygons.

	CSG_IFace face;
	// we may need to decalare some memory for user defined face properties.

	if (props.user_data_size) {
		face.user_face_data = new char[props.user_data_size];
	} else {
		face.user_face_data = NULL;
	}

	if (props.user_face_vertex_data_size) {
		char * fv_data = NULL;
		fv_data = new char[4 * props.user_face_vertex_data_size];
		
		face.user_face_vertex_data[0] = fv_data;
		face.user_face_vertex_data[1] = fv_data + props.user_face_vertex_data_size;
		face.user_face_vertex_data[2] = fv_data + 2*props.user_face_vertex_data_size;
		face.user_face_vertex_data[3] = fv_data + 3*props.user_face_vertex_data_size;
	} else {
		face.user_face_vertex_data[0] = NULL;
		face.user_face_vertex_data[1] = NULL;
		face.user_face_vertex_data[2] = NULL;
		face.user_face_vertex_data[3] = NULL;
	}

		
	int tri_index[3];
	int fv_index[3];

	while (!face_it.Done(face_it.it)) {
		face_it.Fill(face_it.it,&face);

		// Let's not rely on quads being coplanar - especially if they 
		// are coming out of that soup of code from blender...	
		if (face.vertex_number == 4) {
			tri_index[0] = face.vertex_index[2];
			tri_index[1] = face.vertex_index[3];
			tri_index[2] = face.vertex_index[0];

			fv_index[0] = fv_data->Duplicate(face.user_face_vertex_data[2]);
			fv_index[1] = fv_data->Duplicate(face.user_face_vertex_data[3]);
			fv_index[2] = fv_data->Duplicate(face.user_face_vertex_data[0]);

			mesh->AddPolygon(tri_index,fv_index,3);

			// bit of an unspoken relationship between mesh face buffer 
			// and the face data which I guess should be changed.
			face_data->Duplicate(face.user_face_data);

		}

		fv_index[0] = fv_data->Duplicate(face.user_face_vertex_data[0]);
		fv_index[1] = fv_data->Duplicate(face.user_face_vertex_data[1]);
		fv_index[2] = fv_data->Duplicate(face.user_face_vertex_data[2]);

		mesh->AddPolygon(face.vertex_index,fv_index,3);
		// bit of an unspoken relationship between mesh face buffer 
		// and the face data which I guess should be changed.
		face_data->Duplicate(face.user_face_data);


		face_it.Step(face_it.it);
	}

	// give the user face vertex data over to the mesh.

	mesh->SetFaceVertexData(fv_data);	
	mesh->SetFaceData(face_data);

	// that's it 

	delete[] static_cast<char *>(face.user_face_data);
	delete[] static_cast<char *>(face.user_face_vertex_data[0]);
	return mesh;
}

