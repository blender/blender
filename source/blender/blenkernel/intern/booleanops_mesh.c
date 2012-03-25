#if 0

/*
 *
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

/** \file blender/blenkernel/intern/booleanops_mesh.c
 *  \ingroup bke
 */

#include "CSG_BooleanOps.h"





/**
 * Implementation of boolean ops mesh interface.
 */

	void
CSG_DestroyMeshDescriptor(
	CSG_MeshDescriptor *mesh
) {
	// Call mesh descriptors destroy function....
	mesh->m_destroy_func(mesh);
}
	
// Destroy function for blender mesh internals.

static
	void
CSG_DestroyBlenderMeshInternals(
	CSG_MeshDescriptor *mesh
) {
	// Free face and vertex iterators.
	FreeMeshDescriptors(&(mesh->m_face_iterator),&(mesh->m_vertex_iterator));		
}


static
	void
CSG_DestroyCSGMeshInternals(
	CSG_MeshDescriptor *mesh
) {
	CSG_FreeVertexDescriptor(&(mesh->m_vertex_iterator));
	CSG_FreeFaceDescriptor(&(mesh->m_face_iterator));
}

static
	int
MakeCSGMeshFromBlenderBase(
	Base * base,
	CSG_MeshDescriptor * output
) {
	Mesh *me;
	if (output == NULL || base == NULL) return 0;

	me = get_mesh(base->object);
		
	output->m_descriptor.user_face_vertex_data_size = 0;
	output->m_descriptor.user_data_size = sizeof(FaceData);

	output->base = base;
	
	BuildMeshDescriptors(
		base->object,
		&(output->m_face_iterator),
		&(output->m_vertex_iterator)
	);

	output->m_destroy_func = CSG_DestroyBlenderMeshInternals;

	return 1;
}	

	int
CSG_LoadBlenderMesh(
	Object * obj,
	CSG_MeshDescriptor *output
) {

	Mesh *me;
	if (output == NULL || obj == NULL) return 0;

	me = get_mesh(obj);
		
	output->m_descriptor.user_face_vertex_data_size = 0;
	output->m_descriptor.user_data_size = sizeof(FaceData);

	output->base = NULL;
	
	BuildMeshDescriptors(
		obj,
		&(output->m_face_iterator),
		&(output->m_vertex_iterator)
	);

	output->m_destroy_func = CSG_DestroyBlenderMeshInternals;
	output->base = NULL;

	return 1;
}
	



	int
CSG_AddMeshToBlender(
	CSG_MeshDescriptor *mesh
) {
	Mesh *me_new = NULL;
	Object *ob_new = NULL;
	float inv_mat[4][4];

	if (mesh == NULL) return 0;
	if (mesh->base == NULL) return 0;

	invert_m4_m4(inv_mat,mesh->base->object->obmat);

	// Create a new blender mesh object - using 'base' as 
	// a template for the new object.
	ob_new=  AddNewBlenderMesh(mesh->base);

	me_new = ob_new->data;

	// make sure the iterators are reset.
	mesh->m_face_iterator.Reset(mesh->m_face_iterator.it);
	mesh->m_vertex_iterator.Reset(mesh->m_vertex_iterator.it);

	// iterate through results of operation and insert into new object
	// see subsurf.c 

	ConvertCSGDescriptorsToMeshObject(
		ob_new,
		&(mesh->m_descriptor),
		&(mesh->m_face_iterator),
		&(mesh->m_vertex_iterator),
		inv_mat
	);

	return 1;
}

	int 
CSG_PerformOp(
	CSG_MeshDescriptor *mesh1,
	CSG_MeshDescriptor *mesh2,
	int int_op_type,
	CSG_MeshDescriptor *output
) {

	CSG_OperationType op_type;
	CSG_BooleanOperation * bool_op = CSG_NewBooleanFunction();
	int success = 0;

	if (bool_op == NULL) return 0;

	if ((mesh1 == NULL) || (mesh2 == NULL) || (output == NULL)) {
		return 0;
	}	
	if ((int_op_type < 1) || (int_op_type > 3)) return 0;

	switch (int_op_type) {
		case 1 : op_type = e_csg_intersection; break;
		case 2 : op_type = e_csg_union; break;
		case 3 : op_type = e_csg_difference; break;
		case 4 : op_type = e_csg_classify; break;
		default : op_type = e_csg_intersection;
	}
	
	output->m_descriptor = CSG_DescibeOperands(bool_op,mesh1->m_descriptor,mesh2->m_descriptor);
	output->base = mesh1->base;

	if (output->m_descriptor.user_face_vertex_data_size) {
		// Then use the only interp function supported 
		success = 
		CSG_PerformBooleanOperation(
			bool_op,
			op_type,
			mesh1->m_face_iterator,
			mesh1->m_vertex_iterator,
			mesh2->m_face_iterator,
			mesh2->m_vertex_iterator,		
			InterpFaceVertexData	
		);
	}
	else {
		success = 
		CSG_PerformBooleanOperation(
			bool_op,
			op_type,
			mesh1->m_face_iterator,
			mesh1->m_vertex_iterator,
			mesh2->m_face_iterator,
			mesh2->m_vertex_iterator,		
			InterpNoUserData	
		);
	}

	if (!success) {
		CSG_FreeBooleanOperation(bool_op);
		bool_op = NULL;
		return 0;
	}
		
	// get the ouput mesh descriptors.

	CSG_OutputFaceDescriptor(bool_op,&(output->m_face_iterator));
	CSG_OutputVertexDescriptor(bool_op,&(output->m_vertex_iterator));
	output->m_destroy_func = CSG_DestroyCSGMeshInternals;

	return 1;
}

	int
NewBooleanMeshTest(
	struct Base * base,
	struct Base * base_select,
	int op_type
) {

	CSG_MeshDescriptor m1,m2,output;
	CSG_MeshDescriptor output2,output3;
	
	if (!MakeCSGMeshFromBlenderBase(base,&m1)) {
		return 0;
	}
	
	if (!MakeCSGMeshFromBlenderBase(base_select,&m2)) {
		return 0;
	}
	
	CSG_PerformOp(&m1,&m2,1,&output);
	CSG_PerformOp(&m1,&m2,2,&output2);
	CSG_PerformOp(&m1,&m2,3,&output3);

	if (!CSG_AddMeshToBlender(&output)) {
		return 0;
	}
	if (!CSG_AddMeshToBlender(&output2)) {
		return 0;
	}
	if (!CSG_AddMeshToBlender(&output3)) {
		return 0;
	}

	
	CSG_DestroyMeshDescriptor(&m1);
	CSG_DestroyMeshDescriptor(&m2);
	CSG_DestroyMeshDescriptor(&output);
	CSG_DestroyMeshDescriptor(&output2);
	CSG_DestroyMeshDescriptor(&output3);

	return 1;
}

#endif

