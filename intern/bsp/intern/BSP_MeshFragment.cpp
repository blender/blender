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

#include "BSP_MeshFragment.h"

#include "BSP_CSGMesh.h"
#include "MT_Plane3.h"
#include <math.h>

using namespace std;


BSP_MeshFragment::
BSP_MeshFragment(
	BSP_CSGMesh *mesh,
	BSP_Classification classification
):
	m_mesh(mesh),
	m_classification(classification)
{
	MT_assert(m_mesh != NULL);
	//nothing to do
}

const 
	vector<BSP_FaceInd> &
BSP_MeshFragment::
FaceSet(
) const {
	return m_faces;
}

	vector<BSP_FaceInd> &
BSP_MeshFragment::
FaceSet(
) {
	return m_faces;
}

	BSP_CSGMesh *
BSP_MeshFragment::
Mesh(
){
	return m_mesh;
}

	BSP_CSGMesh *
BSP_MeshFragment::
Mesh(
) const {
	return m_mesh;
}

	BSP_Classification 
BSP_MeshFragment::
ClassifyPolygon(
	const MT_Plane3 &plane,
	const BSP_MFace & face,
	std::vector<BSP_MVertex>::const_iterator verts,
	vector<BSP_VertexInd> & visited_verts
){

	vector<BSP_VertexInd>::const_iterator f_vi_end = face.m_verts.end();
	vector<BSP_VertexInd>::const_iterator f_vi_it = face.m_verts.begin();

	BSP_Classification p_class = e_unclassified;

	int on_count = 0;

	for (;f_vi_it != f_vi_end; ++f_vi_it) {

		BSP_MVertex & vert = const_cast<BSP_MVertex &>(verts[int(*f_vi_it)]);	

		if (BSP_Classification(vert.OpenTag()) == e_unclassified)
		{
			MT_Scalar sdistance = plane.signedDistance(vert.m_pos);
			if (fabs(sdistance) <= BSP_SPLIT_EPSILON) {
				// this vertex is on
				vert.SetOpenTag(e_classified_on);	
			} else 
			if (sdistance > MT_Scalar(0)) {
				vert.SetOpenTag(e_classified_out);
			} else {
				vert.SetOpenTag(e_classified_in);
			}
			visited_verts.push_back(*f_vi_it);
		}
		BSP_Classification v_class = BSP_Classification(vert.OpenTag());

		if (v_class == e_classified_on) on_count++;


		if (p_class == e_unclassified || p_class == e_classified_on) {
			p_class = v_class;
		} else 
		if (p_class == e_classified_spanning) {
		} else 
		if (p_class == e_classified_in) {
			if (v_class == e_classified_out) {	
				p_class = e_classified_spanning;
			}
		} else {
			if (v_class == e_classified_in) {
				p_class = e_classified_spanning;
			}
		}
	}

	if (on_count > 2) p_class = e_classified_on;

	return p_class;
}


// Classify this mesh fragment with respect
// to plane. The index sets of this fragment
// are consumed by this action. Vertices
// are tagged with a classification enum.

	void
BSP_MeshFragment::
Classify(
	const MT_Plane3 & plane,
	BSP_MeshFragment * in_frag,
 	BSP_MeshFragment * out_frag,
	BSP_MeshFragment * on_frag,
	vector<BSP_FaceInd> & spanning_faces,
	vector<BSP_VertexInd> & visited_verts
){

	vector<BSP_MVertex> & vertex_set = m_mesh->VertexSet();
	vector<BSP_MFace> & face_set = m_mesh->FaceSet();
	
	// Now iterate through the polygons and classify.

	vector<BSP_FaceInd>::const_iterator fi_end = m_faces.end();
	vector<BSP_FaceInd>::iterator fi_it = m_faces.begin();
			
	vector<BSP_FaceInd> & face_in_set = in_frag->FaceSet();
	vector<BSP_FaceInd> & face_out_set = out_frag->FaceSet();
	vector<BSP_FaceInd> & face_on_set = on_frag->FaceSet();
	
	for (;fi_it != fi_end; ++fi_it) {
			
		BSP_Classification p_class = ClassifyPolygon(
			plane,
			face_set[*fi_it],
			vertex_set.begin(),
			visited_verts
		);
		// p_class now holds the classification for this polygon.
		// assign to the appropriate bucket.

		if (p_class == e_classified_in) {
			face_in_set.push_back(*fi_it);
		} else 
		if (p_class == e_classified_out) {
			face_out_set.push_back(*fi_it);
		} else
		if (p_class == e_classified_on) {
			face_on_set.push_back(*fi_it);
		} else {
			spanning_faces.push_back(*fi_it);
			// This is assigned later when we split the polygons in two.
		}
	}

	m_faces.clear();

}		

	void
BSP_MeshFragment::
Classify(
	BSP_CSGMesh & mesh,
	const MT_Plane3 & plane,
	BSP_MeshFragment * in_frag,
 	BSP_MeshFragment * out_frag,
	BSP_MeshFragment * on_frag,
	vector<BSP_FaceInd> & spanning_faces,
	vector<BSP_VertexInd> & visited_verts
){

	vector<BSP_MVertex> & vertex_set = mesh.VertexSet();
	vector<BSP_MFace> & face_set = mesh.FaceSet();
	
	// Now iterate through the polygons and classify.

	int fi_end = mesh.FaceSet().size();
	int fi_it = 0;
			
	vector<BSP_FaceInd> & face_in_set = in_frag->FaceSet();
	vector<BSP_FaceInd> & face_out_set = out_frag->FaceSet();
	vector<BSP_FaceInd> & face_on_set = on_frag->FaceSet();
	
	for (;fi_it != fi_end; ++fi_it) {
			
		BSP_Classification p_class = ClassifyPolygon(
			plane,
			face_set[fi_it],
			vertex_set.begin(),
			visited_verts
		);
		// p_class now holds the classification for this polygon.
		// assign to the appropriate bucket.

		if (p_class == e_classified_in) {
			face_in_set.push_back(fi_it);
		} else 
		if (p_class == e_classified_out) {
			face_out_set.push_back(fi_it);
		} else
		if (p_class == e_classified_on) {
			face_on_set.push_back(fi_it);
		} else {
			spanning_faces.push_back(fi_it);
		}
	}

}
	void
BSP_MeshFragment::
ClassifyOnFragments(
	const MT_Plane3 &plane,
	BSP_MeshFragment *pos_frag,
	BSP_MeshFragment *neg_frag
){

	vector<BSP_MFace> & face_set = m_mesh->FaceSet();
	vector<BSP_FaceInd>::const_iterator fi_end = m_faces.end();
	vector<BSP_FaceInd>::iterator fi_it = m_faces.begin();

	MT_Scalar d = plane.Scalar();

	for (;fi_it != fi_end; ++fi_it) {
		if (fabs(d + face_set[*fi_it].m_plane.Scalar()) > BSP_SPLIT_EPSILON) {
			pos_frag->FaceSet().push_back(*fi_it);
		} else {
			neg_frag->FaceSet().push_back(*fi_it);
		}
	}	
}

BSP_MeshFragment::
~BSP_MeshFragment(
){
}


