/*
  CSGLib - Software Library for Constructive Solid Geometry
  Copyright (C) 2003-2004  Laurence Bourn

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

  Please send remarks, questions and bug reports to laurencebourn@hotmail.com
*/

/**
 * Implementation of external C api for CSG lib .
 */
#pragma warning( disable : 4786 ) 

#include "CSG_Interface.h"
#include "CSG_Iterator.h"
#include "CSG_BlenderMesh.h"
#include "CSG_MeshBuilder.h"
#include "CSG_CsgOp.h"

#include "MEM_SmartPtr.h"

struct CSG_MeshInfo {
	MEM_SmartPtr<AMesh> output_mesh;
};
	
	CSG_BooleanOperation * 
CSG_NewBooleanFunction(
	void
){
	CSG_MeshInfo * mesh_info = new CSG_MeshInfo;
	CSG_BooleanOperation * output = new CSG_BooleanOperation;

	if (mesh_info==NULL || output==NULL) return NULL;

	mesh_info->output_mesh = NULL;
	output->CSG_info = mesh_info;

	return output;
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

	CSG_MeshInfo * mesh_info = static_cast<CSG_MeshInfo *>(operation->CSG_info);
	if (mesh_info == NULL) return 0;

	obAFaces.Reset(obAFaces.it);
	obBFaces.Reset(obBFaces.it);
	obAVertices.Reset(obAVertices.it);
	obBVertices.Reset(obBVertices.it);

	MEM_SmartPtr<AMesh> outputMesh;

	try {
		// Build the individual meshes
		// set the face data size
		BlenderVProp::InterpFunc = interp_func;

		MEM_SmartPtr<AMesh> obA = MeshBuilder::NewMesh(obAFaces,obAVertices);

		MEM_SmartPtr<AMesh> obB = MeshBuilder::NewMesh(obBFaces,obBVertices);

		if (
			obA == NULL ||
			obB == NULL
		) {
			return 0;
		}

		// build normals
		AMeshWrapper aMeshWrap(obA.Ref()), bMeshWrap(obB.Ref());
		aMeshWrap.ComputePlanes();
		bMeshWrap.ComputePlanes();

		// translate enums!

		switch(op_type) {
			case e_csg_union : 
			case e_csg_classify :
				outputMesh = CsgOp::Union(obA.Ref(),obB.Ref(),true);
				break;
			case e_csg_intersection :
				outputMesh = CsgOp::Intersect(obA.Ref(),obB.Ref(),true);
				break;
			case e_csg_difference :
				outputMesh = CsgOp::Difference(obA.Ref(),obB.Ref(),true);
				break;
			default :
				return 0;
		}
	}
	catch(...) {
		return 0;
	}

	// set the output mesh
	mesh_info->output_mesh = outputMesh;

	return 1;
}

	int
CSG_OutputFaceDescriptor(
	CSG_BooleanOperation * operation,
	CSG_FaceIteratorDescriptor * output
){
	if (operation == NULL) return 0;
	CSG_MeshInfo * mesh_info = static_cast<CSG_MeshInfo *>(operation->CSG_info);

	if (mesh_info == NULL) return 0;
	if (mesh_info->output_mesh == NULL) return 0;

	AMesh_FaceIt_Construct(mesh_info->output_mesh,output);
	return 1;
}


	int
CSG_OutputVertexDescriptor(
	CSG_BooleanOperation * operation,
	CSG_VertexIteratorDescriptor *output
){
	if (operation == NULL) return 0;
	CSG_MeshInfo * mesh_info = static_cast<CSG_MeshInfo *>(operation->CSG_info);

	if (mesh_info == NULL) return 0;
	if (mesh_info->output_mesh == NULL) return 0;

	AMesh_VertexIt_Construct(mesh_info->output_mesh,output);
	return 1;
}

	void
CSG_FreeVertexDescriptor(
	CSG_VertexIteratorDescriptor * v_descriptor
){	
	AMesh_VertexIt_Destruct(v_descriptor);
}	


	void
CSG_FreeFaceDescriptor(
	CSG_FaceIteratorDescriptor * f_descriptor
){
	AMesh_FaceIt_Destruct(f_descriptor);
}


	void
CSG_FreeBooleanOperation(
	CSG_BooleanOperation *operation
){
	if (operation != NULL) {
		CSG_MeshInfo * mesh_info = static_cast<CSG_MeshInfo *>(operation->CSG_info);
		delete(mesh_info);
		delete(operation);
	}
}












