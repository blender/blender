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

#include "BSP_CSGNCMeshSplitter.h"

#include "BSP_CSGMesh.h"
#include "BSP_MeshFragment.h"
#include "BSP_CSGException.h"
#include "MT_MinMax.h"
#include "MT_assert.h"
#include <vector>

using namespace std;

BSP_CSGNCMeshSplitter::
BSP_CSGNCMeshSplitter(
){
	//nothing to do
}

BSP_CSGNCMeshSplitter::
BSP_CSGNCMeshSplitter(
	const BSP_CSGNCMeshSplitter & other
){
	//nothing to do
};


	void
BSP_CSGNCMeshSplitter::
Split(
	const MT_Plane3& plane,
	BSP_MeshFragment *frag,
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *out_frag,
	BSP_MeshFragment *on_frag,
	BSP_MeshFragment *spanning_frag
){
	// First classify the vertices and the polygons of the
	// incoming fragment.
	frag->Classify(plane,in_frag,out_frag,on_frag,m_spanning_faces,m_tagged_verts);

	SplitImp(*(frag->Mesh()),plane,m_spanning_faces,in_frag,out_frag,on_frag,m_tagged_verts);

	m_spanning_faces.clear();
	m_tagged_verts.clear();
}

/// Split the entire mesh with respect to the plane.

	void
BSP_CSGNCMeshSplitter::
Split(
	BSP_CSGMesh & mesh,
	const MT_Plane3& plane,
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *out_frag,
	BSP_MeshFragment *on_frag,
	BSP_MeshFragment *spanning_frag
){
	BSP_MeshFragment::Classify(mesh, plane,in_frag,out_frag,on_frag,m_spanning_faces,m_tagged_verts);

	SplitImp(mesh,plane,m_spanning_faces,in_frag,out_frag,on_frag,m_tagged_verts);
	m_spanning_faces.clear();
	m_tagged_verts.clear();
}


BSP_CSGNCMeshSplitter::
~BSP_CSGNCMeshSplitter(
){
	//nothing to do
}

	void
BSP_CSGNCMeshSplitter::
SplitImp(
	BSP_CSGMesh & mesh,
	const MT_Plane3& plane,
	const std::vector<BSP_FaceInd> & spanning_faces,
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *out_frag,
	BSP_MeshFragment *on_frag,
	std::vector<BSP_VertexInd> & classified_verts
){

	// Just iterate through the spanning faces.
	// Identify the spanning 'edges' and create new vertices 
	// and split the polygons. We make no attempt to share 
	// vertices or preserve edge connectivity or maintain any
	// face properties etc.

	// Assumes you have already classified the vertices.
	// and generated a set of spanning faces.

	vector<BSP_MVertex> & vertex_set = mesh.VertexSet();
	vector<BSP_MFace> & face_set = mesh.FaceSet();

	vector<BSP_FaceInd>::const_iterator sface_end = m_spanning_faces.end();
	vector<BSP_FaceInd>::const_iterator sface_it = m_spanning_faces.begin();

	for (;sface_it != sface_end; ++sface_it) {
		BSP_FaceInd f_in,f_out;
		SplitPolygon(plane,mesh,*sface_it,f_in,f_out);
		
		// Use the open tag to store the original index of the face.
		// This is originally -1.

		if (face_set[*sface_it].OpenTag() == -1) {
			face_set[f_in].SetOpenTag(*sface_it);
			face_set[f_out].SetOpenTag(*sface_it);
		} else {
			face_set[f_in].SetOpenTag(face_set[*sface_it].OpenTag());
			face_set[f_out].SetOpenTag(face_set[*sface_it].OpenTag());
		}

		in_frag->FaceSet().push_back(f_in);
		out_frag->FaceSet().push_back(f_out);
	}
	
	vector<BSP_VertexInd>::const_iterator v_end = classified_verts.end();
	vector<BSP_VertexInd>::const_iterator v_it = classified_verts.begin();

	for (; v_it != v_end; ++v_it) {
		vertex_set[*v_it].SetOpenTag(e_unclassified);
	}
}

	void
BSP_CSGNCMeshSplitter::
SplitPolygon(
	const MT_Plane3& plane,
	BSP_CSGMesh & mesh,
	BSP_FaceInd fi,
	BSP_FaceInd &fin,
	BSP_FaceInd &fout
){

	vector<BSP_MVertex> & vertex_set = mesh.VertexSet();
	vector<BSP_MFace> & face_set = mesh.FaceSet();

	BSP_FaceInd new_fi = face_set.size();
	face_set.push_back(BSP_MFace());

	BSP_MFace & face = face_set[fi];
	BSP_MFace &new_face = face_set[new_fi];

	vector<BSP_VertexInd>::const_iterator f_verts_it = face.m_verts.begin();
	vector<BSP_VertexInd>::const_iterator f_verts_end = face.m_verts.end();
	
	MT_Point3 ptA = vertex_set[face.m_verts.back()].m_pos;
	BSP_Classification prev_class = BSP_Classification(vertex_set[face.m_verts.back()].OpenTag());

	for (; f_verts_it != f_verts_end;++f_verts_it) {

		BSP_Classification v_class = BSP_Classification(vertex_set[*f_verts_it].OpenTag());

		if (v_class == e_classified_on) {
			m_in_loop.push_back(*f_verts_it);
			m_out_loop.push_back(*f_verts_it);
			prev_class = e_classified_on;
			continue;
		} else 
		if (v_class == e_classified_in) {
			m_in_loop.push_back(*f_verts_it);
		} else 
		if (v_class == e_classified_out) {
			m_out_loop.push_back(*f_verts_it);
		}

		if ((prev_class != e_classified_on) &&
			(prev_class != v_class)) {
			// spanning edge

			const MT_Point3 & ptB = vertex_set[*f_verts_it].m_pos;

			// compute the intersection point of plane and ptA->ptB
			MT_Vector3 v = ptB - ptA;
			MT_Scalar sideA = plane.signedDistance(ptA);

			MT_Scalar epsilon = -sideA/plane.Normal().dot(v);
			MT_Point3 new_p = ptA + (v * epsilon);

			BSP_VertexInd new_vi(vertex_set.size());
			vertex_set.push_back(BSP_MVertex(new_p));

			m_in_loop.push_back(new_vi);
			m_out_loop.push_back(new_vi);
		}
		
		ptA = vertex_set[*f_verts_it].m_pos;
		prev_class = v_class;
	}

	// Ok should have 2 loops 1 representing the in_loop and
	// 1 for the out_loop
	
	new_face.m_verts = m_out_loop;
	face.m_verts = m_in_loop;

	new_face.m_plane = face.m_plane;

	// we don't bother maintaining any of the other 
	// properties.
	
	fin = fi;
	fout = new_fi;
	
	m_in_loop.clear();
	m_out_loop.clear();
};


