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

#include "BSP_CSGHelper.h"

#include "BSP_CSGMesh.h"
#include "BSP_MeshFragment.h"
#include "BSP_FragTree.h"
#include "BSP_CSGMeshSplitter.h"
#include "BSP_CSGNCMeshSplitter.h"
#include "BSP_Triangulate.h"

#include "MEM_SmartPtr.h"

using namespace std;

BSP_CSGHelper::
BSP_CSGHelper(
)
{
	// nothing to do
}	

	bool
BSP_CSGHelper::
ComputeOp(
	BSP_CSGMesh * obA,
	BSP_CSGMesh * obB,
	BSP_OperationType op_type, 
	BSP_CSGMesh & output,
	CSG_InterpolateUserFaceVertexDataFunc fv_func
){
	// First work out which parts of polygons we want to keep as we pass stuff 
	// down the tree.

	BSP_Classification e_ATreeB,e_BTreeA;
	bool invertA(false),invertB(false);

	switch (op_type) {
		case e_intern_csg_union :
			e_ATreeB = e_classified_out;
			e_BTreeA = e_classified_out;
			break;
		case e_intern_csg_intersection :
			e_ATreeB = e_classified_in;
			e_BTreeA = e_classified_in;
			break;
		case e_intern_csg_difference :
			invertA = true;
			e_ATreeB = e_classified_in;
			e_BTreeA = e_classified_in;
			break;
		default :
			return false;
	}
	
	if (invertA) {
		obA->Invert();
	}

	if (invertB) {
		obB->Invert();
	}

	MEM_SmartPtr<BSP_CSGMesh> obA_copy = obA->NewCopy();
	MEM_SmartPtr<BSP_CSGMesh> obB_copy = obB->NewCopy();

	// ok we need yet another copy...

	MEM_SmartPtr<BSP_CSGMesh> obA_copy2 = obA->NewCopy();
	MEM_SmartPtr<BSP_CSGMesh> obB_copy2 = obB->NewCopy();

	obA_copy->BuildEdges();
	obB_copy->BuildEdges();

	// Create a splitter to help chop up the mesh and preserrve.
	// mesh connectivity

	MEM_SmartPtr<BSP_CSGMeshSplitter> splitter = new BSP_CSGMeshSplitter(fv_func);
	if (splitter == NULL) return false;

	// Create a splitter to help chop the mesh for tree building.
	MEM_SmartPtr<BSP_CSGNCMeshSplitter> nc_splitter = new BSP_CSGNCMeshSplitter();

	if (splitter == NULL || nc_splitter == NULL) return false;

	// Create a tree for both meshes.

	MEM_SmartPtr<BSP_FragTree> treeA = treeA->New(obA,nc_splitter.Ref());
	MEM_SmartPtr<BSP_FragTree> treeB = treeB->New(obB,nc_splitter.Ref());
	
	if (treeA == NULL || treeB == NULL) {
		return false;
	}

	// Classify each object wrt the other tree.

	MEM_SmartPtr<BSP_MeshFragment>  AinB = new BSP_MeshFragment(obA_copy2,e_classified_in);
	MEM_SmartPtr<BSP_MeshFragment>  AoutB = new BSP_MeshFragment(obA_copy2,e_classified_out);
	MEM_SmartPtr<BSP_MeshFragment>  AonB = new BSP_MeshFragment(obA_copy2,e_classified_on);

	treeB->Classify(obA_copy2,AinB,AoutB,AonB,nc_splitter.Ref());

	MEM_SmartPtr<BSP_MeshFragment>  BinA = new BSP_MeshFragment(obB_copy2,e_classified_in);
	MEM_SmartPtr<BSP_MeshFragment>  BoutA = new BSP_MeshFragment(obB_copy2,e_classified_out);
	MEM_SmartPtr<BSP_MeshFragment>  BonA = new BSP_MeshFragment(obB_copy2,e_classified_on);

	treeA->Classify(obB_copy2,BinA,BoutA,BonA,nc_splitter.Ref());

	// Now we need to work what were the spanning polygons from the original mesh.
	// Build a spanning fragment from them and pass split those mothers.

	MEM_SmartPtr<BSP_MeshFragment>  frag_BTreeA2 = new BSP_MeshFragment(obA_copy,e_BTreeA);
	MEM_SmartPtr<BSP_MeshFragment>  AspanningB = new BSP_MeshFragment(obA_copy,e_classified_spanning);

	TranslateSplitFragments(AinB.Ref(),AoutB.Ref(),AonB.Ref(),e_BTreeA,AspanningB.Ref(),frag_BTreeA2.Ref());
	
	MEM_SmartPtr<BSP_MeshFragment>  frag_ATreeB2 = new BSP_MeshFragment(obB_copy,e_ATreeB);
	MEM_SmartPtr<BSP_MeshFragment>  BspanningA = new BSP_MeshFragment(obB_copy,e_classified_spanning);

	TranslateSplitFragments(BinA.Ref(),BoutA.Ref(),BonA.Ref(),e_ATreeB,BspanningA.Ref(),frag_ATreeB2.Ref());


	MEM_SmartPtr<BSP_MeshFragment>  frag_ATreeB = new BSP_MeshFragment(obB_copy,e_ATreeB);
	MEM_SmartPtr<BSP_MeshFragment>  frag_BTreeA = new BSP_MeshFragment(obA_copy,e_BTreeA);

	if (frag_ATreeB == NULL || frag_BTreeA == NULL) return false;

	// Pass the spanning polygons of copyB through the tree of copyA.
	treeA->Push(BspanningA,frag_ATreeB,e_ATreeB,e_classified_spanning,true,splitter.Ref());

	// Add the result of the push to the fragments we are interested in.
	MergeFrags(frag_ATreeB2.Ref(),frag_ATreeB.Ref());

	// Pass the spanning polygons of copyA through the tree of copyB
	treeB->Push(AspanningB,frag_BTreeA,e_BTreeA,e_classified_spanning,false,splitter.Ref());
	MergeFrags(frag_BTreeA2.Ref(),frag_BTreeA.Ref());

	// Copy the fragments into a new mesh.
	DuplicateMesh(frag_ATreeB.Ref(),output);
	DuplicateMesh(frag_BTreeA.Ref(),output);

	return true;

};

	void
BSP_CSGHelper::
TranslateSplitFragments(
	const BSP_MeshFragment & in_frag,
	const BSP_MeshFragment & out_frag,
	const BSP_MeshFragment & on_frag,
	BSP_Classification keep,
	BSP_MeshFragment & spanning_frag,
	BSP_MeshFragment & output
){
	
	// iterate through the 3 input fragments
	// tag the polygons in the output fragments according to 
	// the classification of the input frags.

	const BSP_CSGMesh *i_mesh = in_frag.Mesh();
	BSP_CSGMesh *o_mesh = output.Mesh();

	const vector<BSP_MFace> &i_faces = i_mesh->FaceSet();
	vector<BSP_MFace> &o_faces = o_mesh->FaceSet();

	vector<BSP_FaceInd>::const_iterator if_it = in_frag.FaceSet().begin();
	vector<BSP_FaceInd>::const_iterator if_end = in_frag.FaceSet().end();
	
	for (;if_it != if_end; ++if_it) {
		int original_index = i_faces[*if_it].OpenTag();
		if (original_index == -1) {
			// then this face was never split and the original_index is
			// the actual face index.
			original_index = *if_it;
		}	
		// tag the output faces with the in flag.
		if (o_faces[original_index].OpenTag() == -1) {
			o_faces[original_index].SetOpenTag(0);
		}
		o_faces[original_index].SetOpenTag(
			o_faces[original_index].OpenTag() | e_classified_in
		);
	}

	// same for out fragments.
	if_it = out_frag.FaceSet().begin();
	if_end = out_frag.FaceSet().end();
	
	for (;if_it != if_end; ++if_it) {
		int original_index = i_faces[*if_it].OpenTag();
		if (original_index == -1) {
			// then this face was never split and the original_index is
			// the actual face index.
			original_index = *if_it;
		}	
		// tag the output faces with the in flag.
		if (o_faces[original_index].OpenTag() == -1) {
			o_faces[original_index].SetOpenTag(0);
		}
		o_faces[original_index].SetOpenTag(
			o_faces[original_index].OpenTag() | e_classified_out
		);
	}
	
	// on fragments just get set as spanning for now.

	if_it = on_frag.FaceSet().begin();
	if_end = on_frag.FaceSet().end();
	
	for (;if_it != if_end; ++if_it) {
		int original_index = i_faces[*if_it].OpenTag();
		if (original_index == -1) {
			// then this face was never split and the original_index is
			// the actual face index.
			original_index = *if_it;
		}	
		// tag the output faces with the in flag.
		if (o_faces[original_index].OpenTag() == -1) {
			o_faces[original_index].SetOpenTag(0);
		}
		o_faces[original_index].SetOpenTag(
			o_faces[original_index].OpenTag() | e_classified_spanning
		);
	}
	// now run through the output faces.
	// collect the ones we are interested in into output 
	// and collect the spanning faces.

	int of_it = 0;
	int of_size = o_faces.size();

	for (;of_it < of_size; ++of_it) {

		int p_class = o_faces[of_it].OpenTag();

		if (p_class == int(keep)) {
			output.FaceSet().push_back(BSP_FaceInd(of_it));
		} else 
		if (
			(p_class == (e_classified_in | e_classified_out)) ||  
			p_class == e_classified_spanning
		) {
			spanning_frag.FaceSet().push_back(BSP_FaceInd(of_it));
		}
	}
}


	void
BSP_CSGHelper::
MergeFrags(
	const BSP_MeshFragment & in,
	BSP_MeshFragment & out
){

	// Add the 2 frags together.

	out.FaceSet().insert(
		out.FaceSet().end(),
		in.FaceSet().begin(),
		in.FaceSet().end()
	);
}



BSP_CSGHelper::
~BSP_CSGHelper(
){
	// nothing to do
}

	void
BSP_CSGHelper::
DuplicateMesh(
	const BSP_MeshFragment & frag,
	BSP_CSGMesh & output
){

	// This stuff is a real waste of time.
	// much better to create an output iterator based upon
	// the 2 mesh fragments alone.

	vector<BSP_MVertex> & o_verts = output.VertexSet();
	BSP_CSGUserData & o_fv_data = output.FaceVertexData();
	BSP_CSGUserData & o_f_data = output.FaceData();
	
	// A temporary buffer containing the triangulated 
	// vertex indices. 

	vector<int> triangle_indices;

	BSP_CSGMesh * i_mesh = frag.Mesh();

	if (i_mesh == NULL) return;

	vector<BSP_MVertex> & i_verts = i_mesh->VertexSet();
	const vector<BSP_MFace> & i_faces = i_mesh->FaceSet();
	BSP_CSGUserData & i_fv_data = i_mesh->FaceVertexData();
	BSP_CSGUserData & i_f_data = i_mesh->FaceData();
	
	// iterate through the fragment's face set 
	const vector<BSP_FaceInd> & frag_faces = frag.FaceSet();

	vector<BSP_FaceInd>::const_iterator f_faces_it = frag_faces.begin();
	vector<BSP_FaceInd>::const_iterator f_faces_end = frag_faces.end();
	
	// We need to keep track of which vertices we are selecting.
	vector<int> selected_vi;

	BSP_Triangulate triangulator;

	for (; f_faces_it != f_faces_end; ++f_faces_it) {
		
		BSP_FaceInd fi = *f_faces_it;
		const BSP_MFace &face = i_faces[fi];

		// duplicate the face
		BSP_MFace dup_face(face);
	
		// iterate through the face's vertex indices.
		vector<BSP_VertexInd>::iterator dup_f_verts_it = dup_face.m_verts.begin();
		vector<BSP_VertexInd>::const_iterator dup_f_verts_end = dup_face.m_verts.end();
 		
		for (; dup_f_verts_it != dup_f_verts_end; ++dup_f_verts_it) {
			
			if (i_verts[*dup_f_verts_it].SelectTag() == false) {
				// copy this vertex onto the output mesh vertex array.

				BSP_VertexInd new_vi(o_verts.size());
				o_verts.push_back(i_verts[*dup_f_verts_it]);
				
				// should kill the old vertices edge ptrs.
				o_verts[new_vi].m_edges.clear();

				// set the open tag in the old vert to the new one.
				i_verts[*dup_f_verts_it].SetOpenTag(new_vi);

				// select the old vertex
				i_verts[*dup_f_verts_it].SetSelectTag(true);
				selected_vi.push_back(*dup_f_verts_it);
			}

			// we have been to this vertex before and there should be 
			// a corresponding vertex in the new mesh vertex set.
			*dup_f_verts_it = i_verts[*dup_f_verts_it].OpenTag();	
		}

		// duplicate the face vertex data for this polygon.

		vector<BSP_UserFVInd>::iterator dup_fv_it = dup_face.m_fv_data.begin();
		vector<BSP_UserFVInd>::const_iterator dup_fv_end = dup_face.m_fv_data.end();
			
		for (;dup_fv_it != dup_fv_end; ++dup_fv_it) {
			*dup_fv_it = o_fv_data.Duplicate(i_fv_data[int(*dup_fv_it)]);
		}

		triangle_indices.clear();

		// Now triangulate the polygon.
		if (!triangulator.Process(
			o_verts,
			dup_face.m_verts,
			dup_face.m_plane,
			triangle_indices
		)) {
			// Sometimes the triangulator can fail for very small
			// polygons or very thing polygons. This should be 
			// handled more elegantly but for now we just leave out the
			// polygon from the mesh.
			continue;
		}

		// Run through the result and add in the triangle faces.

		int i;
		for (i = 0; i < triangle_indices.size(); i+=3) {
			// duplicate the face data for this face.
			o_f_data.Duplicate(i_f_data[*f_faces_it]);

			output.AddSubTriangle(dup_face,triangle_indices.begin() + i); 
		}
	}

	// of course we have to deselect the vertices again.
	
	vector<int>::const_iterator selected_vi_it = selected_vi.begin();
	vector<int>::const_iterator selected_vi_end = selected_vi.end();
			
	for (; selected_vi_it != selected_vi_end; ++selected_vi_it) {
		i_verts[*selected_vi_it].SetSelectTag(false);
	}
}
	






