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

#include "BSP_FragTree.h"

#include "BSP_FragNode.h"
#include "BSP_CSGMesh.h"
#include "BSP_MeshFragment.h"
#include "MT_Plane3.h"
#include "BSP_CSGException.h"
#include <vector>
#include "BSP_CSGISplitter.h"

using namespace std;

	MEM_SmartPtr<BSP_FragTree>
BSP_FragTree::
New(
	BSP_CSGMesh *mesh,
	BSP_CSGISplitter & splitter
){
	if (mesh == NULL) return NULL;
	if (mesh->FaceSet().size() == 0) return NULL;

	// This is the external tree construction method
	// (not the internal method!)
	// We need to build a tree root with an initial
	// node based on the mesh rather than a mesh fragment.

	// For now we pick an arbitrary polygon for the initial
	// plane.

	vector<BSP_MVertex> verts = mesh->VertexSet();
	const BSP_MFace & f0 = mesh->FaceSet()[0]; 	

	const MT_Vector3 & p1 = verts[f0.m_verts[0]].m_pos;
	const MT_Vector3 & p2 = verts[f0.m_verts[1]].m_pos;
	const MT_Vector3 & p3 = verts[f0.m_verts[2]].m_pos;
		
	MT_Plane3 plane = f0.m_plane;

	MEM_SmartPtr<BSP_FragTree> output(new BSP_FragTree(mesh));
	MEM_SmartPtr<BSP_FragNode> node(BSP_FragNode::New(plane,mesh));

	if (output == NULL || node == NULL) return NULL;

	// Generate initial mesh fragments for this plane pass into
	// first node.	
	
	BSP_MeshFragment frag_in(mesh,e_classified_in),frag_out(mesh,e_classified_out);
	
	MEM_SmartPtr<BSP_MeshFragment> on_frag = new BSP_MeshFragment(mesh,e_classified_on);

	splitter.Split(*mesh,plane,&frag_in,&frag_out,on_frag,NULL);

	// We are not interested in the on_frag.
	on_frag.Delete();

	// Build the in_tree of the first node(recursive)
	node->InTree().Build(&frag_in,splitter);

	// Build the out tree of the first node(recursive)
	node->OutTree().Build(&frag_out,splitter);

	output->m_node = node;

	return output;
}
				
	void
BSP_FragTree::
Classify(
	BSP_CSGMesh *mesh,
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *out_frag,
	BSP_MeshFragment *on_frag,
	BSP_CSGISplitter & splitter
){

	if (mesh == NULL) return;
	if (mesh->FaceSet().size() == 0) return;
	if (m_node == NULL) return;

	BSP_MeshFragment frag_in(mesh,e_classified_in);
	BSP_MeshFragment frag_out(mesh,e_classified_out);
	BSP_MeshFragment frag_on(mesh,e_classified_on);
	BSP_MeshFragment frag_spanning(mesh,e_classified_spanning);

	splitter.Split(*mesh,m_node->Plane(),&frag_in,&frag_out,&frag_on,NULL);
	
	if (frag_on.FaceSet().size()) {

		on_frag->FaceSet().insert(
			on_frag->FaceSet().end(),
			frag_on.FaceSet().begin(),
			frag_on.FaceSet().end()
		);

		frag_on.FaceSet().clear();
	}

	// recurse into subtrees.
	m_node->InTree().Classify(&frag_in,in_frag,out_frag,on_frag,e_classified_in,splitter);
	m_node->OutTree().Classify(&frag_out,in_frag,out_frag,on_frag,e_classified_out,splitter);

}	

		



BSP_FragTree::
~BSP_FragTree(
){
	// nothing to do
}

BSP_FragTree::
BSP_FragTree(
	BSP_CSGMesh * mesh
):
	m_mesh(mesh)
{
	//nothing to do
}

BSP_FragTree::
BSP_FragTree(
){
	// nothing to do
}

	void
BSP_FragTree::
Build(
	BSP_MeshFragment * frag,
	BSP_CSGISplitter & splitter
){

	// Node must be NULL because we are building the tree.

	MT_assert(m_node == NULL);

	if (frag->FaceSet().size()) {
		
		// choose a plane for the node. The first index in this
		// mesh fragment will do for now.
		vector<BSP_MVertex> & verts = m_mesh->VertexSet();

		// choose a random splitting plane

		MT_Plane3 plane;
		{
			int rand_index;
#if 1		
			if (frag->FaceSet().size() > 1) {
				rand_index = rand() % frag->FaceSet().size();
			} else {
				rand_index = 0;
			}
#else
			rand_index = 0;
#endif
	
			const BSP_MFace & f0 = m_mesh->FaceSet()[frag->FaceSet()[rand_index]]; 	
			plane = f0.m_plane; 
		}
	
		// build the node.
		m_node = BSP_FragNode::New(plane,frag->Mesh());

		if (m_node == NULL) {
			BSP_CSGException e(e_tree_build_error);
			throw(e);
		}
	
		m_node->Build(frag,splitter);
	}
}


	void
BSP_FragTree::
Push(
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *output,
	const BSP_Classification keep,
	const BSP_Classification current,
	const bool dominant,
	BSP_CSGISplitter & splitter
){

	if (in_frag->FaceSet().size()) {

		if (m_node == NULL) {

			// we have reached a leaf node.
			// if the current classification matches
			// the classification we want to keep
			// copy the polygons of the current
			// fragment onto the output
			vector<BSP_FaceInd>::const_iterator in_frag_it = in_frag->FaceSet().begin();
			vector<BSP_FaceInd>::const_iterator in_frag_end = in_frag->FaceSet().end();
			vector<BSP_MFace>::iterator faces = in_frag->Mesh()->FaceSet().begin();
		
			if (keep == current || current == e_classified_on) {
				for (;in_frag_it != in_frag_end; ++ in_frag_it) {
					output->FaceSet().push_back(*in_frag_it);
				} 

				in_frag->FaceSet().clear();
			} 
		} else {

			m_node->Push(in_frag,output,keep,dominant,splitter);
		}
	}
}


	void
BSP_FragTree::
Classify(
	BSP_MeshFragment * frag,
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *out_frag,
	BSP_MeshFragment *on_frag,
	const BSP_Classification current,
	BSP_CSGISplitter & splitter
){

	if (frag->FaceSet().size()) {

		if (m_node == NULL) {

			vector<BSP_FaceInd>::const_iterator frag_it = frag->FaceSet().begin();
			vector<BSP_FaceInd>::const_iterator frag_end = frag->FaceSet().end();
			
			BSP_MeshFragment *output;
			if (current == e_classified_in) {
				output = in_frag;
			} else {
				//must be e_classified_out
				output = out_frag;
			}
			// Copy the selected indices into the correct output fragment.

			for (;frag_it != frag_end; ++ frag_it) {
				output->FaceSet().push_back(*frag_it);
			} 

			frag->FaceSet().clear();
		} else {

			m_node->Classify(frag,in_frag,out_frag,on_frag,splitter);
		}
	}
}

































