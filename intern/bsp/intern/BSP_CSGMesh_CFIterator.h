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

#ifndef BSP_CSGMesh_CFIterator_h

#define BSP_CSGMesh_CFIterator_h

#include "BSP_CSGMesh.h"
#include "../extern/CSG_BooleanOps.h"
/**
 * This class defines 2 C style iterators over a CSG mesh, one for
 * vertices and 1 for faces. They conform to the iterator interface
 * defined in CSG_BooleanOps.h
 */

struct BSP_CSGMesh_VertexIt {
	MEM_RefCountPtr<BSP_CSGMesh> mesh;
	BSP_MVertex * pos;
};


static
	void
BSP_CSGMesh_VertexIt_Destruct(
	CSG_VertexIteratorDescriptor * iterator
) {
	delete ((BSP_CSGMesh_VertexIt *)(iterator->it));
	iterator->it = NULL;
	iterator->Done = NULL;
	iterator->Fill = NULL;
	iterator->Reset = NULL;
	iterator->Step = NULL;
	iterator->num_elements = 0;
};


static
	int
BSP_CSGMesh_VertexIt_Done(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_VertexIt * vertex_it = (BSP_CSGMesh_VertexIt *)it;

	if (vertex_it->pos < &(*vertex_it->mesh->VertexSet().end())) return 0;
	return 1;
};

static
	void
BSP_CSGMesh_VertexIt_Fill(
	CSG_IteratorPtr it,
	CSG_IVertex *vert
) {
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_VertexIt * vertex_it = (BSP_CSGMesh_VertexIt *)it;
			
	vertex_it->pos->m_pos.getValue(vert->position);
};

static
	void
BSP_CSGMesh_VertexIt_Step(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_VertexIt * vertex_it = (BSP_CSGMesh_VertexIt *)it;

	++(vertex_it->pos);
};

static
	void
BSP_CSGMesh_VertexIt_Reset(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_VertexIt * vertex_it = (BSP_CSGMesh_VertexIt *)it;
	vertex_it->pos = &vertex_it->mesh->VertexSet()[0];
};	

static
	void
BSP_CSGMeshVertexIt_Construct(
	BSP_CSGMesh *mesh,
	CSG_VertexIteratorDescriptor *output
){
	// user should have insured mesh is not equal to NULL.
	
	output->Done = BSP_CSGMesh_VertexIt_Done;
	output->Fill = BSP_CSGMesh_VertexIt_Fill;
	output->Step = BSP_CSGMesh_VertexIt_Step;
	output->Reset = BSP_CSGMesh_VertexIt_Reset;
	output->num_elements = mesh->VertexSet().size();
	
	BSP_CSGMesh_VertexIt * v_it = new BSP_CSGMesh_VertexIt;
	v_it->mesh = mesh;
	v_it->pos = &mesh->VertexSet()[0];
	output->it = v_it;
};			


/**
 * Face iterator.
 */

struct BSP_CSGMesh_FaceIt {
	MEM_RefCountPtr<BSP_CSGMesh> mesh;
	BSP_MFace *pos;
	int face_triangle;
};


static
	void
BSP_CSGMesh_FaceIt_Destruct(
	CSG_FaceIteratorDescriptor *iterator
) {
	delete ((BSP_CSGMesh_FaceIt *)(iterator->it));
	iterator->it = NULL;
	iterator->Done = NULL;
	iterator->Fill = NULL;
	iterator->Reset = NULL;
	iterator->Step = NULL;
	iterator->num_elements = 0;
};


static
	int
BSP_CSGMesh_FaceIt_Done(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_FaceIt * face_it = (BSP_CSGMesh_FaceIt *)it;

	if (face_it->pos < &(*face_it->mesh->FaceSet().end())) {
		if (face_it->face_triangle + 3 <= face_it->pos->m_verts.size()) {
			return 0;
		}
	}
	return 1;
};

static
	void
BSP_CSGMesh_FaceIt_Fill(
	CSG_IteratorPtr it,
	CSG_IFace *face
){
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_FaceIt * face_it = (BSP_CSGMesh_FaceIt *)it;		
	// essentially iterating through a triangle fan here.
	const int tri_index = face_it->face_triangle;

	face->vertex_index[0] = int(face_it->pos->m_verts[0]);
	face->vertex_index[1] = int(face_it->pos->m_verts[tri_index + 1]);
	face->vertex_index[2] = int(face_it->pos->m_verts[tri_index + 2]);

	// Copy the user face data across - this does nothing
	// if there was no mesh user data.

	// time to change the iterator type to an integer...
	face_it->mesh->FaceData().Copy(
		face->user_face_data,
		int(face_it->pos - &face_it->mesh->FaceSet()[0])
	);
	
	// Copy face vertex data across...

	face_it->mesh->FaceVertexData().Copy(
		face->user_face_vertex_data[0],
		face_it->pos->m_fv_data[0]
	);

	face_it->mesh->FaceVertexData().Copy(
		face->user_face_vertex_data[1],
		face_it->pos->m_fv_data[tri_index + 1]
	);

	face_it->mesh->FaceVertexData().Copy(
		face->user_face_vertex_data[2],
		face_it->pos->m_fv_data[tri_index + 2]
	);

	face->vertex_number = 3;
};

static
	void
BSP_CSGMesh_FaceIt_Step(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_FaceIt * face_it = (BSP_CSGMesh_FaceIt *)it;		

	// safety guard
	if (face_it->pos < &(*face_it->mesh->FaceSet().end())) {

		if (face_it->face_triangle + 3 < face_it->pos->m_verts.size()) {
			(face_it->face_triangle)++;
		} else {
			face_it->face_triangle = 0;
			(face_it->pos) ++;
		}
	}
};

static
	void
BSP_CSGMesh_FaceIt_Reset(
	CSG_IteratorPtr it
) {
	// assume CSG_IteratorPtr is of the correct type.
	BSP_CSGMesh_FaceIt * f_it = (BSP_CSGMesh_FaceIt *)it;		
	f_it->pos = &f_it->mesh->FaceSet()[0];
	f_it->face_triangle = 0;
};

static
	void
BSP_CSGMesh_FaceIt_Construct(
	BSP_CSGMesh * mesh,
	CSG_FaceIteratorDescriptor *output
) {

	output->Done = BSP_CSGMesh_FaceIt_Done;
	output->Fill = BSP_CSGMesh_FaceIt_Fill;
	output->Step = BSP_CSGMesh_FaceIt_Step;
	output->Reset = BSP_CSGMesh_FaceIt_Reset;

	output->num_elements = mesh->CountTriangles();
	
	BSP_CSGMesh_FaceIt * f_it = new BSP_CSGMesh_FaceIt;
	f_it->mesh = mesh;
	f_it->pos = &mesh->FaceSet()[0];
	f_it->face_triangle = 0;

	output->it = f_it;

};


#endif

