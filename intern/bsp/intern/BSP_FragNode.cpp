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

#include "BSP_CSGMesh.h"

#include "BSP_FragNode.h"
#include "BSP_CSGISplitter.h"


BSP_FragNode::
BSP_FragNode(
	const MT_Plane3 & plane,
	BSP_CSGMesh *mesh
):
	m_plane(plane),
	m_in_tree(mesh),
	m_out_tree(mesh)
{
}

/**
 * Public methods
 * Should only be called by BSP_FragTree
 */

BSP_FragNode::
~BSP_FragNode(
){
	// nothing to do
}

	MEM_SmartPtr<BSP_FragNode>
BSP_FragNode::
New(
	const MT_Plane3 & plane,
	BSP_CSGMesh *mesh
){
	return new BSP_FragNode(plane,mesh);
}


	void
BSP_FragNode::
Build(
	BSP_MeshFragment *frag,
	BSP_CSGISplitter & splitter
){
	// we know there must be some polygons still in
	// the fragment otherwise this node would not hve been
	// constructed.

	BSP_CSGMesh *mesh = frag->Mesh();

	// split the incoming fragment by the plane
	// generating in,out,on fragments which are
	// passed down the in and out trees.

	BSP_MeshFragment in_frag(mesh,e_classified_in),out_frag(mesh,e_classified_out);
	MEM_SmartPtr<BSP_MeshFragment> on_frag = new BSP_MeshFragment(mesh,e_classified_on);
	splitter.Split(m_plane,frag,&in_frag,&out_frag,on_frag,NULL);

	// We are not interested in the on fragments.
	on_frag.Delete();

	m_in_tree.Build(&in_frag,splitter);
	m_out_tree.Build(&out_frag,splitter);
}	

	void
BSP_FragNode::
Push(
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *output,
	const BSP_Classification keep,
	const bool dominant,
	BSP_CSGISplitter & splitter
){		
	BSP_CSGMesh *mesh = in_frag->Mesh();


	MEM_SmartPtr<BSP_MeshFragment> inside_frag = new BSP_MeshFragment(mesh,e_classified_in);
	MEM_SmartPtr<BSP_MeshFragment> outside_frag = new BSP_MeshFragment(mesh,e_classified_out);
	MEM_SmartPtr<BSP_MeshFragment> on_frag = new BSP_MeshFragment(mesh,e_classified_on);

	// deal with memory exceptions here.

	splitter.Split(m_plane,in_frag,inside_frag,outside_frag,on_frag,NULL);

	// deal with the on_fragments.

	if (on_frag->FaceSet().size()) {
	
		// The on fragment contains polygons that are outside both subtrees and polygons
		// that are inside one or more sub trees. If we are taking the union then we can 				
		// immediately add that first set of polygons to the ouput. We must then decide what
		// to do with potenially overlapping polygons from both objects. If we assume both 
		// objects are closed then we can identify the conflict zones as
		// polygons outside B- and inside B+ 
		// polygons outside B+ and inside B-
		
		// In these conflict zones we must choose a dominant object this is indicated
		// by the bool parameter to this function. If the object is not dominant then
		// we do nothing inside these conflict zones.
		// The first set should correspond with on polygons from object B with the same
		// orientation as this node. The second corresponding with polygons with opposite
		// orientation. 
		// We don't want to replace polygons from A with polygons of opposite orientation
		// from B. So we split up the on polygons of A into 2 sets according to their orientation.
		// We add to output (A- out B-) in B+ and (A+ out B+) in B- 
	
	
#if 1   

		if (keep == e_classified_out) {
			// we are doing a union operation.
			// Make sure that this is not a leaf node.
			if(m_in_tree.m_node != NULL || m_out_tree.m_node != NULL) {
				BSP_MeshFragment frag_outBneg_outBpos(mesh,e_classified_on);
				BSP_MeshFragment temp1(on_frag.Ref());
				m_in_tree.Push(
					&temp1,&frag_outBneg_outBpos,
					e_classified_out,e_classified_on,
					false,splitter
				);
			
				m_out_tree.Push(
					&frag_outBneg_outBpos,output,e_classified_out,e_classified_on,
					false,splitter
				);
			}		
#if 1
			if (dominant) {
			
				// Here we compute the intersection zones.
				BSP_MeshFragment frag_on_pos(mesh,e_classified_on),frag_on_neg(mesh,e_classified_on);
				on_frag->ClassifyOnFragments(m_plane,&frag_on_pos,&frag_on_neg);
			
				BSP_MeshFragment temp1(mesh,e_classified_in);
				
				// push -ve fragments down inside tree, push result down outside
				m_in_tree.Push(&frag_on_neg,&temp1,e_classified_out,e_classified_on,false,splitter);
				m_out_tree.Push(&temp1,output,e_classified_in,e_classified_on,false,splitter);
				temp1.FaceSet().clear();
		
				// push +ve fragments down outside tree, push result down inside.
				m_out_tree.Push(&frag_on_pos,&temp1,e_classified_out,e_classified_on,false,splitter);
				m_in_tree.Push(&temp1,output,e_classified_in,e_classified_on,false,splitter);
			}
#endif
		} else if (keep == e_classified_in) {

			// we are doing an intersection
			
			// A = on_frag in X+ out X-
			// B = on_frag in X- out X+
			// C = on_frag in X- in X+
			
			// If X+ is NULL then A = F out X-, B = 0, C = F in X-
			// If X- is NULLL then A = 0, B = F out X+ , C = F in X+
			// If both NULL then A = C = 0, B = F
			
			// Conflicts only happen in A and B.
			// negative fragments only in A, positive fragments only in B, anything in C.
			// First compute F in C an add to ouput.
			
			BSP_MeshFragment frag_on_pos(mesh,e_classified_on),frag_on_neg(mesh,e_classified_on);
			on_frag->ClassifyOnFragments(m_plane,&frag_on_pos,&frag_on_neg);
						
			if (m_in_tree.m_node == NULL) {
				if (m_out_tree.m_node == NULL) {
					// pick stuff that points in the same direction as this node
					// only if priority.
					if (dominant) {
						// pass +ve frags into B = F.
						// trick just pass down in tree... just adds to output.
						m_in_tree.Push(&frag_on_pos,output,e_classified_in,e_classified_on,false,splitter);
					}					
				} else {
					// A = 0, B= F out X+ , C = F in X+
					if (dominant) {
					//	m_out_tree.Push(&frag_on_pos,output,e_classified_out,e_classified_on,false,splitter);
						m_out_tree.Push(on_frag,output,e_classified_in,e_classified_on,false,splitter);
					}
				}
			} else {
				if (m_out_tree.m_node == NULL) {
					// A = F out X-, B=0, C = F in X-
					if (dominant) {
					//	m_in_tree.Push(&frag_on_neg,output,e_classified_out,e_classified_on,false,splitter);
						m_in_tree.Push(on_frag,output,e_classified_in,e_classified_on,false,splitter);
					}
				} else {
					// The normals case
					if (dominant) {
						BSP_MeshFragment temp1(mesh,e_classified_on);
						m_out_tree.Push(&frag_on_neg,&temp1,e_classified_in,e_classified_on,false,splitter);
						m_in_tree.Push(&temp1,output,e_classified_out,e_classified_on,false,splitter);
						temp1.FaceSet().clear();
						
						m_in_tree.Push(&frag_on_pos,&temp1,e_classified_in,e_classified_on,false,splitter);
						m_out_tree.Push(&temp1,output,e_classified_out,e_classified_on,false,splitter);
					}
					BSP_MeshFragment temp1(mesh,e_classified_on);
					m_in_tree.Push(on_frag,&temp1,e_classified_in,e_classified_on,false,splitter);
					m_out_tree.Push(&temp1,output,e_classified_in,e_classified_on,false,splitter);
				}
			}
		}		
					
							
#endif
		on_frag.Delete();
	}

	m_in_tree.Push(inside_frag,output,keep,e_classified_in,dominant,splitter);
	m_out_tree.Push(outside_frag,output,keep,e_classified_out,dominant,splitter);
};

	void
BSP_FragNode::
Classify(
	BSP_MeshFragment * frag,
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *out_frag,
	BSP_MeshFragment *on_frag,
	BSP_CSGISplitter & splitter
){

	BSP_CSGMesh *mesh = frag->Mesh();

	MEM_SmartPtr<BSP_MeshFragment> inside_frag = new BSP_MeshFragment(mesh,e_classified_in);
	MEM_SmartPtr<BSP_MeshFragment> outside_frag = new BSP_MeshFragment(mesh,e_classified_out);
	MEM_SmartPtr<BSP_MeshFragment> frag_on = new BSP_MeshFragment(mesh,e_classified_on);

	splitter.Split(m_plane,frag,inside_frag,outside_frag,frag_on,NULL);

	// copy the on fragments into the on_frag output.

	if (frag_on->FaceSet().size()) {

		on_frag->FaceSet().insert(
			on_frag->FaceSet().end(),
			frag_on->FaceSet().begin(),
			frag_on->FaceSet().end()
		);
	}		

	frag_on.Delete();

	// pass everything else down the tree.

	m_in_tree.Classify(inside_frag,in_frag,out_frag,on_frag,e_classified_in,splitter);
	m_out_tree.Classify(outside_frag,in_frag,out_frag,on_frag,e_classified_out,splitter);
}


/**
 * Accessor methods
 */

	BSP_FragTree &
BSP_FragNode::
InTree(
){
	return m_in_tree;
}

	BSP_FragTree &
BSP_FragNode::
OutTree(
){
	return m_out_tree;
}
	
	MT_Plane3&
BSP_FragNode::
Plane(
){
	return m_plane;
}





