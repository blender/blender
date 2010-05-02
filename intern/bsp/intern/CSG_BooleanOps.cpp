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

/**

 * Implementation of external api for CSG part of BSP lib interface.
 */

#include "../extern/CSG_BooleanOps.h"
#include "BSP_CSGMesh_CFIterator.h"
#include "MEM_RefCountPtr.h"

#include "../../boolop/extern/BOP_Interface.h"
#include <iostream>
using namespace std;

#include "BSP_MeshPrimitives.h"

struct BSP_MeshInfo {
	BSP_CSGMesh *output_mesh;
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
	
/**
 * Compute the boolean operation, UNION, INTERSECION or DIFFERENCE
 */
	int
CSG_PerformBooleanOperation(
	CSG_BooleanOperation			*operation,
	CSG_OperationType				op_type,
	CSG_FaceIteratorDescriptor		obAFaces,
	CSG_VertexIteratorDescriptor	obAVertices,
	CSG_FaceIteratorDescriptor		obBFaces,
	CSG_VertexIteratorDescriptor	obBVertices
){
	if (operation == NULL) return 0;
	BSP_MeshInfo * mesh_info = static_cast<BSP_MeshInfo *>(operation->CSG_info);
	if (mesh_info == NULL) return 0;

	obAFaces.Reset(obAFaces.it);
	obBFaces.Reset(obBFaces.it);
	obAVertices.Reset(obAVertices.it);
	obBVertices.Reset(obBVertices.it);

	BoolOpType boolType;
	
	switch( op_type ) {
	case e_csg_union:
	  boolType = BOP_UNION;
	  break;
	case e_csg_difference:
	  boolType = BOP_DIFFERENCE;
	  break;
	default:
	  boolType = BOP_INTERSECTION;
	  break;
	}

	BoolOpState boolOpResult;
	try {
	boolOpResult = BOP_performBooleanOperation( boolType,
				     (BSP_CSGMesh**) &(mesh_info->output_mesh),
					 obAFaces, obAVertices, obBFaces, obBVertices);
	}
	catch(...) {
		return 0;
	}

	switch (boolOpResult) {
	case BOP_OK: return 1;
	case BOP_NO_SOLID: return -2;
	case BOP_ERROR: return 0;
	default: return 1;
	}
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

		delete (mesh_info->output_mesh);
		delete(mesh_info);
		delete(operation);
	}
}

