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
#pragma warning( disable : 4786 ) 

#include "CSG_CsgOp.h"
#include "CSG_BooleanOp.h"
#include "CSG_BBoxTree.h"
#include "CSG_Math.h"
#include "CSG_GeometryBinder.h"
#include "CSG_MeshCopier.h"

#include "MEM_SmartPtr.h"

using namespace std;

	void
BuildTree(
	const AMesh& mesh,
	BBoxTree& tree
) {
	int numLeaves = mesh.Polys().size();

	BBoxLeaf *aLeaves = new BBoxLeaf[numLeaves];

	int i;
	for (i=0;i< mesh.Polys().size(); i++) 
	{
		PolygonGeometry<AMesh> pg(mesh,i);
		aLeaves[i] = BBoxLeaf(i,CSG_Math<PolygonGeometry<AMesh> >::FitBBox(pg) );
	}
	
	tree.BuildTree(aLeaves,numLeaves);
}


	void
ExtractClassificationPreserve(
	const AMesh& meshA,
	const AMesh& meshB,
	const BBoxTree& aTree,
	const BBoxTree& bTree,
	const OverlapTable& aOverlapsB,
	const OverlapTable& bOverlapsA,
	int aClassification,
	int bClassification,
	bool reverseA,
	bool reverseB,
	AMesh& output
){

	// Now we need to make a copy of each mesh and then partition each of those
	// copies with respect to the original meshes.
	AConnectedMesh meshAPartitioned;
	AConnectedMesh meshBPartitioned;

	MeshCopier<AMesh,AConnectedMesh>::Copy(meshA,meshAPartitioned);
	MeshCopier<AMesh,AConnectedMesh>::Copy(meshB,meshBPartitioned);

	AConnectedMeshWrapper meshAWrapper(meshAPartitioned);
	AConnectedMeshWrapper meshBWrapper(meshBPartitioned);

	meshAWrapper.BuildVertexPolyLists();
	meshBWrapper.BuildVertexPolyLists();
	// Partition meshA wrt to meshB
	BooleanOp<AConnectedMeshWrapper,AMesh>::PartitionMesh(meshAWrapper,meshB,bOverlapsA);
	// and now meshB wrt meshA
	BooleanOp<AConnectedMeshWrapper,AMesh>::PartitionMesh(meshBWrapper,meshA,aOverlapsB);

	// Classify the partioned meshes wrt to the originals.
	BooleanOp<AConnectedMesh,AMesh>::ClassifyMesh(meshB,bTree,meshAPartitioned);
	BooleanOp<AConnectedMesh,AMesh>::ClassifyMesh(meshA,aTree,meshBPartitioned);

	// Extract the classification we want from both meshes
	BooleanOp<AConnectedMesh,AMesh>::ExtractClassification(meshAPartitioned,	output,aClassification,reverseA);
	BooleanOp<AConnectedMesh,AMesh>::ExtractClassification(meshBPartitioned,	output,bClassification,reverseB);
}

	void
ExtractClassification(
	const AMesh& meshA,
	const AMesh& meshB,
	const BBoxTree& aTree,
	const BBoxTree& bTree,
	const OverlapTable& aOverlapsB,
	const OverlapTable& bOverlapsA,
	int aClassification,
	int bClassification,
	bool reverseA,
	bool reverseB,
	AMesh& output
){
	// Now we need to make a copy of each mesh and then partition each of those
	// copies with respect to the original meshes.
	AMesh meshAPartitioned(meshA);
	AMesh meshBPartitioned(meshB);

	AMeshWrapper meshAWrapper(meshAPartitioned);
	AMeshWrapper meshBWrapper(meshBPartitioned);

	// Partition meshA wrt to meshB
	BooleanOp<AMeshWrapper,AMesh>::PartitionMesh(meshAWrapper,meshB,bOverlapsA);
	// and now meshB wrt meshA
	BooleanOp<AMeshWrapper,AMesh>::PartitionMesh(meshBWrapper,meshA,aOverlapsB);

	// Classify the partioned meshes wrt to the originals.
	BooleanOp<AMesh,AMesh>::ClassifyMesh(meshB,bTree,meshAPartitioned);
	BooleanOp<AMesh,AMesh>::ClassifyMesh(meshA,aTree,meshBPartitioned);

	// Extract the classification we want from both meshes
	BooleanOp<AMesh,AMesh>::ExtractClassification(meshAPartitioned,	output,aClassification,reverseA);
	BooleanOp<AMesh,AMesh>::ExtractClassification(meshBPartitioned,	output,bClassification,reverseB);
}

	AMesh *
CsgOp::
Intersect(
	const AMesh& meshA,
	const AMesh& meshB,
	bool preserve
){
	// assumes they occups the same space and their planes have
	// been computed.

	// First thing is to build a BBoxTree for each mesh.
	BBoxTree aTree,bTree;
	BuildTree(meshA,aTree);
	BuildTree(meshB,bTree);

	// Build the overlap tables - they tell us which polygons from 
	// meshA overlap with those of meshB and vice versa	
	OverlapTable bOverlapsA(meshA.Polys().size());
	OverlapTable aOverlapsB(meshB.Polys().size());

	BooleanOp<AMesh,AMesh>::BuildSplitGroup(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA);

	// Create a new mesh for the output
	MEM_SmartPtr<AMesh> output = new AMesh;
	if (output == NULL) return NULL;

	if (preserve) 
	{
		ExtractClassificationPreserve(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA,1,1,false,false,output.Ref());
	} else {
		ExtractClassification(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA,1,1,false,false,output.Ref());
	}

#if 1
	// Triangulate the result
	AMeshWrapper outputWrapper(output.Ref());
	outputWrapper.Triangulate();
#endif
	return output.Release();	
}


	AMesh *
CsgOp::
Union(
	const AMesh& meshA,
	const AMesh& meshB,
	bool preserve
){
	// assumes they occups the same space and their planes have
	// been computed.

	// First thing is to build a BBoxTree for each mesh.
	BBoxTree aTree,bTree;
	BuildTree(meshA,aTree);
	BuildTree(meshB,bTree);

	// Build the overlap tables - they tell us which polygons from 
	// meshA overlap with those of meshB and vice versa	
	OverlapTable bOverlapsA(meshA.Polys().size());
	OverlapTable aOverlapsB(meshB.Polys().size());

	BooleanOp<AMesh,AMesh>::BuildSplitGroup(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA);

	// Create a new mesh for the output
	MEM_SmartPtr<AMesh> output = new AMesh;
	if (output == NULL) return NULL;

	if (preserve) 
	{
		ExtractClassificationPreserve(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA,2,2,false,false,output.Ref());
	} else {
		ExtractClassification(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA,2,2,false,false,output.Ref());
	}

#if 1
	// Triangulate the result
	AMeshWrapper outputWrapper(output.Ref());
	outputWrapper.Triangulate();
#endif
	return output.Release();	
}

	AMesh *
CsgOp::
Difference(
	const AMesh& meshA,
	const AMesh& meshB,
	bool preserve
){

	// assumes they occups the same space and their planes have
	// been computed.

	// First thing is to build a BBoxTree for each mesh.
	BBoxTree aTree,bTree;
	BuildTree(meshA,aTree);
	BuildTree(meshB,bTree);

	// Build the overlap tables - they tell us which polygons from 
	// meshA overlap with those of meshB and vice versa	
	OverlapTable bOverlapsA(meshA.Polys().size());
	OverlapTable aOverlapsB(meshB.Polys().size());

	BooleanOp<AMesh,AMesh>::BuildSplitGroup(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA);

	// Create a new mesh for the output
	MEM_SmartPtr<AMesh> output = new AMesh;
	if (output == NULL) return NULL;

	if (preserve) 
	{
		ExtractClassificationPreserve(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA,2,1,false,true,output.Ref());
	} else {
		ExtractClassification(meshA,meshB,aTree,bTree,aOverlapsB,bOverlapsA,2,1,false,true,output.Ref());
	}

#if 1
	// Triangulate the result
	AMeshWrapper outputWrapper(output.Ref());
	outputWrapper.Triangulate();
#endif
	return output.Release();	
}


