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

/**

 * Implementation of external api for CSG part of BSP lib interface.
 */

#include "../extern/CSG_BooleanOps.h"
#include "BSP_CSGMesh_CFIterator.h"
#include "BSP_CSGMeshBuilder.h"
#include "BSP_CSGHelper.h"
#include "BSP_CSGUserData.h"
#include "MEM_RefCountPtr.h"

struct BSP_MeshInfo {
	MEM_RefCountPtr<BSP_CSGMesh> output_mesh;
	CSG_MeshPropertyDescriptor obA_descriptor;
	CSG_MeshPropertyDescriptor obB_descriptor;
	CSG_MeshPropertyDescriptor output_descriptor;
};

using namespace std;
	
	CSG_BooleanOperation * 
CSG_NewBooleanFunction(
	void
){
	BSP_MeshInfo * mesh_info = new BSP_MeshInfo;
	CSG_BooleanOperation * output = new CSG_BooleanOperation;

	if (mesh_info==NULL || output==NULL) return NULL;

	mesh_info->output_mesh = NULL;
	output->CSG_info = mesh_info;

	return output;
}
	
	CSG_MeshPropertyDescriptor
CSG_DescibeOperands(
	CSG_BooleanOperation * operation,
	CSG_MeshPropertyDescriptor operandA_desciption,
	CSG_MeshPropertyDescriptor operandB_desciption
){
	BSP_MeshInfo * mesh_info = static_cast<BSP_MeshInfo *>(operation->CSG_info);

	mesh_info->obA_descriptor = operandA_desciption;
	mesh_info->obB_descriptor = operandB_desciption;

	if (
		(operandA_desciption.user_data_size == operandB_desciption.user_data_size) &&
		(operandA_desciption.user_face_vertex_data_size == operandB_desciption.user_face_vertex_data_size)
	) {
		// Then both operands have the same sets of data we can cope with this!
		mesh_info->output_descriptor.user_data_size = operandA_desciption.user_data_size;
		mesh_info->output_descriptor.user_face_vertex_data_size = operandA_desciption.user_face_vertex_data_size;
	} else {
		// There maybe some common subset of data we can seperate out but for now we just use the 
		// default 
		mesh_info->output_descriptor.user_data_size = 0;
		mesh_info->output_descriptor.user_face_vertex_data_size = 0;
	}
	return mesh_info->output_descriptor;
}

	int
CSG_PerformBooleanOperation(
	CSG_BooleanOperation * operation,
	CSG_OperationType op_type,
	CSG_FaceIteratorDescriptor obAFaces,
	CSG_VertexIteratorDescriptor obAVertices,
	CSG_FaceIteratorDescriptor obBFaces,
	CSG_VertexIteratorDescriptor obBVertices,
	CSG_InterpolateUserFaceVertexDataFunc interp_func
){
	
	if (operation == NULL) return 0;
	BSP_MeshInfo * mesh_info = static_cast<BSP_MeshInfo *>(operation->CSG_info);
	if (mesh_info == NULL) return 0;

	bool success = 0;

	obAFaces.Reset(obAFaces.it);
	obBFaces.Reset(obBFaces.it);
	obAVertices.Reset(obAVertices.it);
	obBVertices.Reset(obBVertices.it);


	try {
		// Build the individual meshes
		
		MEM_SmartPtr<BSP_CSGMesh> obA = 
			BSP_CSGMeshBuilder::NewMesh(mesh_info->obA_descriptor,obAFaces,obAVertices);

		MEM_SmartPtr<BSP_CSGMesh> obB = 
			BSP_CSGMeshBuilder::NewMesh(mesh_info->obB_descriptor,obBFaces,obBVertices);

		// create an empty vertex array for the output mesh 
		MEM_SmartPtr<vector<BSP_MVertex> > output_verts(new vector<BSP_MVertex>);
		// and some user data arrays matching the output descriptor

		MEM_SmartPtr<BSP_CSGUserData> output_f_data = new BSP_CSGUserData(
			mesh_info->output_descriptor.user_data_size
		);
		MEM_SmartPtr<BSP_CSGUserData> output_fv_data = new BSP_CSGUserData(
			mesh_info->output_descriptor.user_face_vertex_data_size
		);

		// create the output mesh!
		mesh_info->output_mesh = BSP_CSGMesh::New();

		if (
			obA == NULL ||
			obB == NULL ||
			output_verts == NULL ||
			mesh_info->output_mesh == NULL ||
			output_f_data == NULL ||
			output_fv_data == NULL
		) {
			return 0;
		}

		mesh_info->output_mesh->SetVertices(output_verts);
		mesh_info->output_mesh->SetFaceData(output_f_data);
		mesh_info->output_mesh->SetFaceVertexData(output_fv_data);

		BSP_CSGHelper helper;
		// translate enums!

		switch(op_type) {
			case e_csg_union : 
			case e_csg_classify :
				success = helper.ComputeOp(obA,obB,e_intern_csg_union,
					mesh_info->output_mesh.Ref(),interp_func
				);
				break;
			case e_csg_intersection :
				success = helper.ComputeOp(obA,obB,e_intern_csg_intersection,
					mesh_info->output_mesh.Ref(),interp_func
				);
				break;
			case e_csg_difference :
				success = helper.ComputeOp(obA,obB,e_intern_csg_difference,
					mesh_info->output_mesh.Ref(),interp_func
				);
				break;
			default :
				success = 0;
		}
	}
	catch(...) {
		return 0;
	}

	return success;
}

	int
CSG_OutputFaceDescriptor(
	CSG_BooleanOperation * operation,
	CSG_FaceIteratorDescriptor * output
){
	if (operation == NULL) return 0;
	BSP_MeshInfo * mesh_info = static_cast<BSP_MeshInfo *>(operation->CSG_info);

	if (mesh_info == NULL) return 0;
	if (mesh_info->output_mesh == NULL) return 0;

	BSP_CSGMesh_FaceIt_Construct(mesh_info->output_mesh,output);
	return 1;
}


	int
CSG_OutputVertexDescriptor(
	CSG_BooleanOperation * operation,
	CSG_VertexIteratorDescriptor *output
){
	if (operation == NULL) return 0;
	BSP_MeshInfo * mesh_info = static_cast<BSP_MeshInfo *>(operation->CSG_info);

	if (mesh_info == NULL) return 0;
	if (mesh_info->output_mesh == NULL) return 0;

	BSP_CSGMeshVertexIt_Construct(mesh_info->output_mesh,output);
	return 1;
}

	void
CSG_FreeVertexDescriptor(
	CSG_VertexIteratorDescriptor * v_descriptor
){	
	BSP_CSGMesh_VertexIt_Destruct(v_descriptor);
}	


	void
CSG_FreeFaceDescriptor(
	CSG_FaceIteratorDescriptor * f_descriptor
){
	BSP_CSGMesh_FaceIt_Destruct(f_descriptor);
}


	void
CSG_FreeBooleanOperation(
	CSG_BooleanOperation *operation
){
	if (operation != NULL) {
		BSP_MeshInfo * mesh_info = static_cast<BSP_MeshInfo *>(operation->CSG_info);
		delete(mesh_info);
		delete(operation);
	}
}












