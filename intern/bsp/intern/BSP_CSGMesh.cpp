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
#include "MT_assert.h"
#include "CTR_TaggedSetOps.h"
#include "BSP_MeshFragment.h"
#include "MT_Plane3.h"
#include "BSP_CSGException.h"

// for vector reverse
#include <algorithm>
using namespace std;

BSP_CSGMesh::
BSP_CSGMesh(
) :
	MEM_RefCountable()
{
    // nothing to do
}

	BSP_CSGMesh *
BSP_CSGMesh::
New(
){
	return new BSP_CSGMesh();
}

	BSP_CSGMesh *
BSP_CSGMesh::
NewCopy(
) const {

	MEM_SmartPtr<BSP_CSGMesh> mesh = New();
	if (mesh == NULL) return NULL;

	mesh->m_bbox_max = m_bbox_max;
	mesh->m_bbox_min = m_bbox_min;

	if (m_edges != NULL) {
		mesh->m_edges = new vector<BSP_MEdge>(m_edges.Ref());
		if (mesh->m_edges == NULL) return NULL;
	}
	if (m_verts != NULL) {
		mesh->m_verts = new vector<BSP_MVertex>(m_verts.Ref());
		if (mesh->m_verts == NULL) return NULL;
	}
	if (m_faces != NULL) {
		mesh->m_faces = new vector<BSP_MFace>(m_faces.Ref());
		if (mesh->m_faces == NULL) return NULL;
	}
	if (m_fv_data != NULL) {
		mesh->m_fv_data = new BSP_CSGUserData(m_fv_data.Ref());
		if (mesh->m_fv_data == NULL) return NULL;
	}
	if (m_face_data != NULL) {
		mesh->m_face_data = new BSP_CSGUserData(m_face_data.Ref());
		if (mesh->m_face_data == NULL) return NULL;
	}

	return mesh.Release();
}
	
	void
BSP_CSGMesh::
Invert(
){

	vector<BSP_MFace> & faces = FaceSet();

	vector<BSP_MFace>::const_iterator faces_end = faces.end();
	vector<BSP_MFace>::iterator faces_it = faces.begin();

	for (; faces_it != faces_end; ++faces_it) {	
		faces_it->Invert();
	}
}

	bool
BSP_CSGMesh::
SetVertices(
	MEM_SmartPtr<vector<BSP_MVertex> > verts
){
	if (verts == NULL) return false;

	// create polygon and edge arrays and reserve some space.
	m_faces = new vector<BSP_MFace>;
	if (!m_faces) return false;

	m_faces->reserve(verts->size()/2);
	
	// previous verts get deleted here.
	m_verts = verts;
	return true;
}

	void
BSP_CSGMesh::
SetFaceVertexData(
	MEM_SmartPtr<BSP_CSGUserData> fv_data
){
	m_fv_data = fv_data;
}

	void
BSP_CSGMesh::
SetFaceData(
	MEM_SmartPtr<BSP_CSGUserData> f_data
) {
	m_face_data = f_data;
}


		void
BSP_CSGMesh::
AddPolygon(
	const int * verts,
	int num_verts
){
	MT_assert(verts != NULL);
	MT_assert(num_verts >=3);

	if (verts == NULL || num_verts <3) return;

	const int vertex_num = m_verts->size();

	// make a polyscone from these vertex indices.

	const BSP_FaceInd fi = m_faces->size();
	m_faces->push_back(BSP_MFace());			
	BSP_MFace & face = m_faces->back();

	insert_iterator<vector<BSP_VertexInd> > insert_point(face.m_verts,face.m_verts.end()); 
	copy (verts,verts + num_verts,insert_point);

	// compute and store the plane equation for this face.

	MT_Plane3 face_plane = FacePlane(fi);	
	face.m_plane = face_plane;
};

	void
BSP_CSGMesh::
AddPolygon(
	const int * verts,
	const int * fv_indices,
	int num_verts
){
	// This creates a new polygon on the end of the face list.
	AddPolygon(verts,num_verts);

	BSP_MFace & face = m_faces->back();
	// now we just fill in the fv indices

	if (fv_indices) {
		insert_iterator<vector<BSP_UserFVInd> > insert_point(face.m_fv_data,face.m_fv_data.end());
		copy(fv_indices,fv_indices + num_verts,insert_point);
	} else {
		face.m_fv_data.insert(face.m_fv_data.end(),num_verts,BSP_UserFVInd::Empty());
	}
}


	void
BSP_CSGMesh::
AddSubTriangle(
	const BSP_MFace &iface,
	const int * index_info
){
	// This creates a new polygon on the end of the face list.

	const BSP_FaceInd fi = m_faces->size();
	m_faces->push_back(BSP_MFace());			
	BSP_MFace & face = m_faces->back();

	face.m_verts.push_back(iface.m_verts[index_info[0]]);
	face.m_verts.push_back(iface.m_verts[index_info[1]]);
	face.m_verts.push_back(iface.m_verts[index_info[2]]);

	face.m_fv_data.push_back(iface.m_fv_data[index_info[0]]);
	face.m_fv_data.push_back(iface.m_fv_data[index_info[1]]);
	face.m_fv_data.push_back(iface.m_fv_data[index_info[2]]);

	face.m_plane = iface.m_plane;
}

	
// assumes that the face already has a plane equation
	void
BSP_CSGMesh::
AddPolygon(
	const BSP_MFace &face
){
	m_faces->push_back(face);			
};


	bool
BSP_CSGMesh::
BuildEdges(
){

	if (m_faces == NULL) return false;

	if (m_edges != NULL) {
		DestroyEdges();
	}

	m_edges = new vector<BSP_MEdge>;

	if (m_edges == NULL) {
		return false;
	}
		

	//iterate through the face set and add edges for all polygon
	//edges
	
	vector<BSP_MFace>::const_iterator f_it_end = FaceSet().end();
	vector<BSP_MFace>::const_iterator f_it_begin = FaceSet().begin();
	vector<BSP_MFace>::iterator f_it = FaceSet().begin();

	vector<BSP_MVertex> & vertex_set = VertexSet();

	vector<BSP_EdgeInd> dummy;

	for (;f_it != f_it_end; ++f_it) {

		BSP_MFace & face = *f_it;

		int vertex_num = face.m_verts.size();
		BSP_VertexInd prev_vi(face.m_verts[vertex_num-1]);

		for (int vert = 0; vert < vertex_num; ++vert) {

			BSP_FaceInd fi(f_it - f_it_begin);
			InsertEdge(prev_vi,face.m_verts[vert],fi,dummy);
			prev_vi = face.m_verts[vert];
		}
			
	}
	dummy.clear();
	return true;
}	
		
	void
BSP_CSGMesh::
DestroyEdges(
){
	m_edges.Delete();
	
	// Run through the vertices
	// and clear their edge arrays.

	if (m_verts){

		vector<BSP_MVertex>::const_iterator vertex_end = VertexSet().end();
		vector<BSP_MVertex>::iterator vertex_it = VertexSet().begin();

		for (; vertex_it != vertex_end; ++vertex_it) {
			vertex_it->m_edges.clear();
		}
	}
}


	BSP_EdgeInd
BSP_CSGMesh::
FindEdge(
	const BSP_VertexInd & v1,
	const BSP_VertexInd & v2
) const {
	
	vector<BSP_MVertex> &verts = VertexSet();
	vector<BSP_MEdge> &edges = EdgeSet();

	BSP_MEdge e;
	e.m_verts[0] = v1;
	e.m_verts[1] = v2;
	
	vector<BSP_EdgeInd> &v1_edges = verts[v1].m_edges;
	vector<BSP_EdgeInd>::const_iterator v1_end = v1_edges.end();
	vector<BSP_EdgeInd>::const_iterator v1_begin = v1_edges.begin();

	for (; v1_begin != v1_end; ++v1_begin) {
		if (edges[*v1_begin] == e) return *v1_begin;
	}
	
	return BSP_EdgeInd::Empty();
}

	void
BSP_CSGMesh::
InsertEdge(
	const BSP_VertexInd & v1,
	const BSP_VertexInd & v2,
	const BSP_FaceInd & f,
	vector<BSP_EdgeInd> &new_edges
){

	MT_assert(!v1.IsEmpty());
	MT_assert(!v2.IsEmpty());
	MT_assert(!f.IsEmpty());

	if (v1.IsEmpty() || v2.IsEmpty() || f.IsEmpty()) {
		BSP_CSGException e(e_mesh_error);
		throw (e);
	}

	vector<BSP_MVertex> &verts = VertexSet();
	vector<BSP_MEdge> &edges = EdgeSet();
	
	BSP_EdgeInd e;

	e = FindEdge(v1,v2);
	if (e.IsEmpty()) {
		// This edge does not exist -- make a new one 

		BSP_MEdge temp_e;
		temp_e.m_verts[0] = v1;
		temp_e.m_verts[1] = v2;

		e = m_edges->size();
		// set the face index from the edge back to this polygon.
		temp_e.m_faces.push_back(f);

		m_edges->push_back(temp_e);

		// add the edge index to it's vertices 
		verts[v1].AddEdge(e);
		verts[v2].AddEdge(e);

		new_edges.push_back(e);

	} else {

		// edge already exists
		// insure that there is no polygon already
		// attached to the other side of this edge
		// swap the empty face pointer in edge with f

		BSP_MEdge &edge = edges[e];

		// set the face index from the edge back to this polygon.
		edge.m_faces.push_back(f);
	}
}		


// geometry access
//////////////////

	vector<BSP_MVertex> &
BSP_CSGMesh::
VertexSet(
) const {
	return m_verts.Ref();
}		

	vector<BSP_MFace> &
BSP_CSGMesh::
FaceSet(
) const {
	return m_faces.Ref();
}

	vector<BSP_MEdge> &
BSP_CSGMesh::
EdgeSet(
) const {
	return m_edges.Ref();
}

	BSP_CSGUserData &
BSP_CSGMesh::
FaceVertexData(
) const {
	return m_fv_data.Ref();
}

	BSP_CSGUserData &
BSP_CSGMesh::
FaceData(
) const {
	return m_face_data.Ref();
}


BSP_CSGMesh::
~BSP_CSGMesh(
){
	// member deletion handled by smart ptr;
}

// local geometry queries.
/////////////////////////

// face queries
///////////////

	void
BSP_CSGMesh::
FaceVertices(
	const BSP_FaceInd & f,
	vector<BSP_VertexInd> &output
){
	vector<BSP_MFace> & face_set = FaceSet();
	output.insert(
		output.end(),
		face_set[f].m_verts.begin(),
		face_set[f].m_verts.end()
	);
}


	void
BSP_CSGMesh::
FaceEdges(
	const BSP_FaceInd & fi,
	vector<BSP_EdgeInd> &output
){
	// take intersection of the edges emminating from all the vertices
	// of this polygon;

	vector<BSP_MFace> &faces = FaceSet();
	vector<BSP_MEdge> &edges = EdgeSet();

	const BSP_MFace & f = faces[fi];

	// collect vertex edges;

	vector<BSP_VertexInd>::const_iterator face_verts_it = f.m_verts.begin();
	vector<BSP_VertexInd>::const_iterator face_verts_end = f.m_verts.end();

	vector< vector<BSP_EdgeInd> > vertex_edges(f.m_verts.size());
		
	int vector_slot = 0;

	for (;face_verts_it != face_verts_end; ++face_verts_it, ++vector_slot) {
		VertexEdges(*face_verts_it,vertex_edges[vector_slot]);
	}	

	int prev = vector_slot - 1; 

	// intersect pairs of edge sets 

	for (int i = 0; i < vector_slot;i++) {
		CTR_TaggedSetOps<BSP_EdgeInd,BSP_MEdge>::IntersectPair(vertex_edges[prev],vertex_edges[i],edges,output);	
		prev = i;
	}
	
	// should always have 3 or more unique edges per face.
	MT_assert(output.size() >=3);

	if (output.size() < 3) {
		BSP_CSGException e(e_mesh_error);
		throw(e);
	}
};
	
// edge queries
///////////////

	void
BSP_CSGMesh::
EdgeVertices(
	const BSP_EdgeInd & e,
	vector<BSP_VertexInd> &output
){
	const vector<BSP_MEdge> &edges = EdgeSet();
	output.push_back(edges[e].m_verts[0]);
	output.push_back(edges[e].m_verts[1]);
} 

	void
BSP_CSGMesh::
EdgeFaces(
	const BSP_EdgeInd & e,
	vector<BSP_FaceInd> &output
){

	vector<BSP_MEdge> & edge_set = EdgeSet();
	output.insert(
		output.end(),
		edge_set[e].m_faces.begin(),
		edge_set[e].m_faces.end()
	);
	
}

// vertex queries
/////////////////

	void
BSP_CSGMesh::
VertexEdges(
	const BSP_VertexInd &v,
	vector<BSP_EdgeInd> &output
){

	vector<BSP_MVertex> & vertex_set = VertexSet();
	output.insert(
		output.end(),
		vertex_set[v].m_edges.begin(),
		vertex_set[v].m_edges.end()
	);
}

	void
BSP_CSGMesh::
VertexFaces(
	const BSP_VertexInd &vi,
	vector<BSP_FaceInd> &output
) {

	vector<BSP_MEdge> &edges = EdgeSet();
	vector<BSP_MFace> &faces = FaceSet();
	vector<BSP_MVertex> &verts = VertexSet();

	const vector<BSP_EdgeInd> &v_edges = verts[vi].m_edges;
	vector<BSP_EdgeInd>::const_iterator e_it = v_edges.begin();

	for (; e_it != v_edges.end(); ++e_it) {

		BSP_MEdge &e = edges[*e_it]; 

		// iterate through the faces of this edge - push unselected
		// edges to ouput and then select the edge

		vector<BSP_FaceInd>::const_iterator e_faces_end = e.m_faces.end();
		vector<BSP_FaceInd>::iterator e_faces_it = e.m_faces.begin();

		for (;e_faces_it != e_faces_end; ++e_faces_it) {

			if (!faces[*e_faces_it].SelectTag()) {
				output.push_back(*e_faces_it);
				faces[*e_faces_it].SetSelectTag(true);
			}
		}
	}

	// deselect selected faces.
	vector<BSP_FaceInd>::iterator f_it = output.begin();

	for (; f_it != output.end(); ++f_it) {
		faces[*f_it].SetSelectTag(false);
	}
}

	void
BSP_CSGMesh::
InsertVertexIntoFace(
	BSP_MFace & face,
	const BSP_VertexInd & v1,
	const BSP_VertexInd & v2,
	const BSP_VertexInd & vi,
	CSG_InterpolateUserFaceVertexDataFunc fv_split_func,
	MT_Scalar epsilon
){
	// We assume that the face vertex data indices
	// are consistent with the vertex inidex data.

	// look for v1
	vector<BSP_VertexInd>::iterator result = 
		find(face.m_verts.begin(),face.m_verts.end(),v1);
	
	MT_assert(result != face.m_verts.end());
	
	BSP_CSGUserData & fv_data = m_fv_data.Ref();

	// now we have to check on either side of the result for the 
	// other vertex
	
	vector<BSP_VertexInd>::iterator prev = result - 1;
	if (prev < face.m_verts.begin()) {	
		prev = face.m_verts.end() -1;
	}
	if (*prev == v2) {

		// so result <=> v2 and prev <=> v1

		// create space for new face vertex data
		
		int vf_i = fv_data.Size();
		fv_data.IncSize();

		int vf_i2 = prev - face.m_verts.begin();
		int vf_i1 = result - face.m_verts.begin();

		(*fv_split_func)(
			fv_data[int(face.m_fv_data[vf_i1])],
			fv_data[int(face.m_fv_data[vf_i2])],
			fv_data[vf_i],
			epsilon
		);
	
		// insert vertex data index.
		face.m_fv_data.insert(face.m_fv_data.begin() + vf_i1,vf_i);
		face.m_verts.insert(result,vi);

		return;
	}

	vector<BSP_VertexInd>::iterator next = result + 1;
	if (next >= face.m_verts.end()) {	
		next = face.m_verts.begin();
	}
	if (*next == v2) {

		// so result <=> v1 and next <=> v2

		int vf_i = fv_data.Size();
		fv_data.IncSize();

		int vf_i2 = int(next - face.m_verts.begin());
		int vf_i1 = int(result - face.m_verts.begin());

		(*fv_split_func)(
			fv_data[int(face.m_fv_data[vf_i1])],
			fv_data[int(face.m_fv_data[vf_i2])],
			fv_data[vf_i],
			epsilon
		);

		// insert vertex data index.
		face.m_fv_data.insert(face.m_fv_data.begin() + vf_i2,vf_i);
		face.m_verts.insert(next,vi);

		return;
	}

	// if we get here we are in trouble.
	MT_assert(false);
	BSP_CSGException e(e_mesh_error);
	throw(e);
}

	void
BSP_CSGMesh::
SetBBox(
	const MT_Vector3 & min,
	const MT_Vector3 & max
){
	m_bbox_min = min;
	m_bbox_max = max;
}


	void
BSP_CSGMesh::
BBox(
	MT_Vector3 &min,
	MT_Vector3 &max
) const {
	min = m_bbox_min;
	max = m_bbox_max;
}


// Update the BBox
//////////////////

	void
BSP_CSGMesh::
UpdateBBox(
){
	// TODO
};

	void
BSP_CSGMesh::
SC_Classification(
	BSP_FaceInd f,
	const MT_Plane3& plane
){
	const BSP_MFace & face = FaceSet()[f];

	vector<BSP_VertexInd>::const_iterator f_verts_it = face.m_verts.begin();
	vector<BSP_VertexInd>::const_iterator f_verts_end = face.m_verts.end();

	for (;f_verts_it != f_verts_end; ++f_verts_it) {

		const BSP_MVertex & vert = VertexSet()[*f_verts_it];

		MT_Scalar dist = plane.signedDistance(
			vert.m_pos
		);

		if (fabs(dist) <= BSP_SPLIT_EPSILON ){
			MT_assert(BSP_Classification(vert.OpenTag()) == e_classified_on);
		} else
		if (dist > BSP_SPLIT_EPSILON) {
			MT_assert(BSP_Classification(vert.OpenTag()) == e_classified_out);
		} else 
		if (dist < BSP_SPLIT_EPSILON) {
			MT_assert(BSP_Classification(vert.OpenTag()) == e_classified_in);
		}
	}
}


	bool
BSP_CSGMesh::
SC_Face(
	BSP_FaceInd f
){



#if 0
	{
	// check area is greater than zero.

		vector<BSP_MVertex> & verts = VertexSet();

		vector<BSP_VertexInd> f_verts;
		FaceVertices(f,f_verts);

		MT_assert(f_verts.size() >= 3);

		BSP_VertexInd root = f_verts[0];
		
		MT_Scalar area = 0;

		for (int i=2; i < f_verts.size(); i++) {
			MT_Vector3 a = verts[root].m_pos;
			MT_Vector3 b = verts[f_verts[i-1]].m_pos;
			MT_Vector3 c = verts[f_verts[i]].m_pos;

			MT_Vector3 l1 = b-a;
			MT_Vector3 l2 = c-b;

			area += (l1.cross(l2)).length()/2;
		}

		MT_assert(!MT_fuzzyZero(area));
	}
#endif
	// Check coplanarity 
#if 0
	MT_Plane3 plane = FacePlane(f);

	const BSP_MFace & face = FaceSet()[f];
	vector<BSP_VertexInd>::const_iterator f_verts_it = face.m_verts.begin();
	vector<BSP_VertexInd>::const_iterator f_verts_end = face.m_verts.end();

	for (;f_verts_it != f_verts_end; ++f_verts_it) {
		MT_Scalar dist = plane.signedDistance(
			VertexSet()[*f_verts_it].m_pos
		);

		MT_assert(fabs(dist) < BSP_SPLIT_EPSILON);
	}
#endif


	// Check connectivity

	vector<BSP_EdgeInd> f_edges;	
	FaceEdges(f,f_edges);

	MT_assert(f_edges.size() == FaceSet()[f].m_verts.size());

	unsigned int i;
	for (i = 0; i < f_edges.size(); ++i) {

		int matches = 0;
		for (unsigned int j = 0; j < EdgeSet()[f_edges[i]].m_faces.size(); j++) {

			if (EdgeSet()[f_edges[i]].m_faces[j] == f) matches++;
		}

		MT_assert(matches == 1);

	}	
	return true;
}	

	MT_Plane3
BSP_CSGMesh::
FacePlane(
	const BSP_FaceInd & fi
) const{

	const BSP_MFace & f0 = FaceSet()[fi]; 	

	// Have to be a bit careful here coz the poly may have 
	// a lot of parallel edges. Should walk round the polygon
	// and check length of cross product.

	const MT_Vector3 & p1 = VertexSet()[f0.m_verts[0]].m_pos;
	const MT_Vector3 & p2 = VertexSet()[f0.m_verts[1]].m_pos;

	int face_size = f0.m_verts.size();
	MT_Vector3 n;

	for (int i = 2 ; i <face_size; i++) { 
		const MT_Vector3 & pi =  VertexSet()[f0.m_verts[i]].m_pos;
		
		MT_Vector3 l1 = p2-p1;
		MT_Vector3 l2 = pi-p2;
		n = l1.cross(l2);
		MT_Scalar length = n.length();

		if (!MT_fuzzyZero(length)) {
			n = n * (1/length);
			break;
		} 
	}
	return MT_Plane3(n,p1);
}

	void
BSP_CSGMesh::
ComputeFacePlanes(
){

	int fsize = FaceSet().size();
	int i=0;
	for (i = 0; i < fsize; i++) {
	
		FaceSet()[i].m_plane = FacePlane(i);
	}
};


	int
BSP_CSGMesh::
CountTriangles(
) const {

	// Each polygon of n sides can be partitioned into n-3 triangles.
	// So we just go through and sum this function of polygon size.
	
	vector<BSP_MFace> & face_set = FaceSet();

	vector<BSP_MFace>::const_iterator face_it = face_set.begin();
	vector<BSP_MFace>::const_iterator face_end = face_set.end();

	int sum = 0;

	for (;face_it != face_end; face_it++) {
	
		// Should be careful about degenerate faces here.
		sum += face_it->m_verts.size() - 2;
	}

	return sum;
}



