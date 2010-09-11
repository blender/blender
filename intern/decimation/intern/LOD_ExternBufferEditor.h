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

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 */

#ifndef NAN_INCLUDED_LOD_ExternBufferEditor_h
#define NAN_INCLUDED_LOD_ExternBufferEditor_h

#include "LOD_MeshPrimitives.h"
#include <vector>
#include "LOD_ManMesh2.h"
#include "../extern/LOD_decimation.h"


// This class syncs external vertex/face buffers 
// with the internal mesh representation during
// decimation.

class LOD_ExternBufferEditor 
{

public :

	static 
		LOD_ExternBufferEditor *
	New(
		LOD_Decimation_InfoPtr extern_info
	){
		if (extern_info == NULL) return NULL;
		return new LOD_ExternBufferEditor(extern_info);
	}
	
	// update the external vertex buffer with vertices
	// from the mesh

		void
	CopyModifiedVerts(
		LOD_ManMesh2 & mesh,
		const std::vector<LOD_VertexInd> & mod_vertices
	){
	
		std::vector<LOD_VertexInd>::const_iterator v_start = mod_vertices.begin();
		std::vector<LOD_VertexInd>::const_iterator v_end = mod_vertices.end();

		std::vector<LOD_Vertex> & mesh_verts = mesh.VertexSet();

		float * const extern_vertex_ptr = m_extern_info->vertex_buffer;

		for (; v_start != v_end; ++v_start) {
			float * mod_vert = extern_vertex_ptr + int(*v_start)*3;
			mesh_verts[*v_start].CopyPosition(mod_vert);
		}
	}			
		
	// update the external face buffer with faces from the mesh

		void
	CopyModifiedFaces(
		LOD_ManMesh2 & mesh,
		const std::vector<LOD_FaceInd> & mod_faces
	){
	
		std::vector<LOD_FaceInd>::const_iterator f_start = mod_faces.begin();
		std::vector<LOD_FaceInd>::const_iterator f_end = mod_faces.end();

		std::vector<LOD_TriFace> &mesh_faces = mesh.FaceSet();

		int * const extern_face_ptr = m_extern_info->triangle_index_buffer;

		for (; f_start != f_end; ++f_start) {
			int *mod_face = extern_face_ptr + 3*int(*f_start);
			mesh_faces[*f_start].CopyVerts(mod_face);
		}
	}


	// Copy the last vertex over the vertex specified by
	// vi. Decrement the size of the vertex array	

		void
	CopyBackVertex(
		LOD_VertexInd vi
	){

		float * const extern_vertex_ptr = m_extern_info->vertex_buffer;
		int * extern_vertex_num = &(m_extern_info->vertex_num);

		float * last_external_vert = extern_vertex_ptr + 3*((*extern_vertex_num) - 1);
		float * external_vert = extern_vertex_ptr + 3*int(vi);

		external_vert[0] = last_external_vert[0];
		external_vert[1] = last_external_vert[1];
		external_vert[2] = last_external_vert[2];

		*extern_vertex_num -=1;
	}

	// Copy the last face over the face specified by fi
	// Decrement the size of the face array

		void
	CopyBackFace(
		LOD_FaceInd fi
	) {
		int * const extern_face_ptr = m_extern_info->triangle_index_buffer;
		int * extern_face_num = &(m_extern_info->face_num);
		
		int * last_external_face = extern_face_ptr + 3*((*extern_face_num) -1);
		int * external_face = extern_face_ptr + 3*int(fi);
		external_face[0] = last_external_face[0];
		external_face[1] = last_external_face[1];
		external_face[2] = last_external_face[2];

		*extern_face_num -=1;
	}	


private :
		
	LOD_ExternBufferEditor(
		LOD_Decimation_InfoPtr extern_info	
	) :
		m_extern_info (extern_info)
	{
	}

	LOD_Decimation_InfoPtr const m_extern_info;

};

#endif

