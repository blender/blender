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

#include "BSP_CSGMeshSplitter.h"

#include "BSP_CSGMesh.h"
#include "BSP_MeshFragment.h"
#include "BSP_CSGException.h"
#include "MT_MinMax.h"
#include "MT_assert.h"

using namespace std;


BSP_CSGMeshSplitter::
BSP_CSGMeshSplitter(
	CSG_InterpolateUserFaceVertexDataFunc fv_split_func
) : m_fv_func(fv_split_func)
{
	// nothing to do
};

	void
BSP_CSGMeshSplitter::
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

	void
BSP_CSGMeshSplitter::
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

BSP_CSGMeshSplitter::
~BSP_CSGMeshSplitter(
){
	// nothing to do
}

	void
BSP_CSGMeshSplitter::
SplitImp(
	BSP_CSGMesh & mesh,
	const MT_Plane3& plane,
	const std::vector<BSP_FaceInd> & spanning_faces,
	BSP_MeshFragment *in_frag,
	BSP_MeshFragment *out_frag,
	BSP_MeshFragment *on_frag,
	std::vector<BSP_VertexInd> & classified_verts
){
	// Assumes you have already classified the vertices.
	// and generated a set of spanning faces.

	vector<BSP_MEdge> & edge_set = mesh.EdgeSet();
	vector<BSP_MVertex> & vertex_set = mesh.VertexSet();
	vector<BSP_MFace> & face_set = mesh.FaceSet();

	// Now identify the spanning edges.
	// These can be computed in many ways but probably the most
	// efficient is to select all edges from the vertices of the 
	// spanning polygons that cross the plane.

	vector<BSP_FaceInd>::const_iterator sface_end = m_spanning_faces.end();
	vector<BSP_FaceInd>::const_iterator sface_it = m_spanning_faces.begin();

	for (;sface_it != sface_end; ++sface_it) {
	
		BSP_MFace & sface = face_set[*sface_it];
		
		vector<BSP_VertexInd>::const_iterator sf_vert_end = sface.m_verts.end();
		vector<BSP_VertexInd>::iterator sf_vert_it = sface.m_verts.begin();

		for (;sf_vert_it != sf_vert_end; ++sf_vert_it) {
			BSP_MVertex & vert = vertex_set[*sf_vert_it];

			if (!vert.SelectTag()) {
				// what classification does this vertex have?

				BSP_Classification root_vert_class = BSP_Classification(vert.OpenTag());
					
				// we are only interested in edges whose vertices are in and out.
				if (root_vert_class != e_classified_on) {
	
					BSP_Classification opp_class = e_classified_out;
					if (root_vert_class == e_classified_out) {
						opp_class = e_classified_in;
					}				
					// we haven't visited this vertex before so lets 
					// analyse it's edges.

					vector<BSP_EdgeInd>::const_iterator v_edge_end = vert.m_edges.end();
					vector<BSP_EdgeInd>::iterator v_edge_it = vert.m_edges.begin();

					for (; v_edge_it != v_edge_end; ++v_edge_it) {
						BSP_MEdge & edge = edge_set[*v_edge_it];
			
						if (!edge.SelectTag()) {
							// we haven't visited this edge before so check it's
							// end points 

							// we know that a spanning polygon can have at most 
							// 2 on vertices (even at this point where we haven't
							// yet split the edge!) We are interested in edges whose
							// vertices are in and out the plane.

							BSP_VertexInd opp_vi = edge.OpVertex(*sf_vert_it);
							if (vertex_set[opp_vi].OpenTag() == opp_class) {
								// we have found an edge !!!!
								m_spanning_edges.push_back(*v_edge_it);
							}
							edge.SetSelectTag(true);
							m_visited_edges.push_back(*v_edge_it);
						}
					}
				}

				vert.SetSelectTag(true);
				m_visited_verts.push_back(*sf_vert_it);
			}
		}
	}

	// clear the tags we used in the above 

	unsigned int i;

	for (i = 0; i < m_visited_edges.size(); ++i) {
		edge_set[m_visited_edges[i]].SetSelectTag(false);
	}
	for (i=0;i < m_visited_verts.size(); ++i) {
		vertex_set[m_visited_verts[i]].SetSelectTag(false);
	}
	for (i=0; i < m_spanning_faces.size(); ++i) {
		face_set[m_spanning_faces[i]].SetSelectTag(false);
	}

	// The spanning edge set constains unique edges. Next we run
	// through the edge set and compute the intersection with the 
	// plane --- the edge is guarenteed not to be parallel to the plane!
	// we then split the edge with the new vertex.

	// We identify the polygons affected by the split 

	vector<BSP_EdgeInd>::const_iterator s_edge_end = m_spanning_edges.end();
	vector<BSP_EdgeInd>::iterator s_edge_it = m_spanning_edges.begin();
	
	for (;s_edge_it != s_edge_end; ++s_edge_it) {

		const BSP_MEdge & edge = edge_set[*s_edge_it];

		const BSP_MVertex &v1 = vertex_set[edge.m_verts[0]];
		const BSP_MVertex &v2 = vertex_set[edge.m_verts[1]];
	
		const MT_Vector3 & ptA = v1.m_pos;
		const MT_Vector3 & ptB = v2.m_pos;

		// compute the intersection point of plane and ptA->ptB
		MT_Vector3 v = ptB - ptA;
		MT_Scalar sideA = plane.signedDistance(ptA);

		MT_Scalar epsilon = -sideA/plane.Normal().dot(v);
		MT_Vector3 new_p = ptA + (v * epsilon);
		
		// so new_p is the intersection of the plane and the edge.	
		// split the edge at new_p

		BSP_MVertex new_vert;
		new_vert.m_pos = new_p;
		
		BSP_VertexInd new_vi = SplitEdge(mesh,*s_edge_it,new_vert,epsilon);

		// tag the new vertex as 'on' the plane - we use this information
		// to split the affected polygons below.
		vertex_set[new_vi].SetOpenTag(e_classified_on);
	
		// We add it to the tagged verts so we can remove the tag later.
		classified_verts.push_back(new_vi);
	}

	// We start with the spanning polygons...
	// not forgetting to add the new polygon fragments to the correct fragment bins.

	sface_end = m_spanning_faces.end();
	sface_it = m_spanning_faces.begin();

	for (;sface_it != sface_end; ++sface_it) {

		BSP_FaceInd f_in,f_out;

		SplitPolygon(mesh,*sface_it,f_in,f_out);
		in_frag->FaceSet().push_back(f_in);
		out_frag->FaceSet().push_back(f_out);
	}

	// Finally we have to clean up the vertex tags we set on all the vertices 
	// There will be some overlap between the vertex sets, so this operation 
	// is a tiny bit inefficient.

	vector<BSP_VertexInd>::const_iterator v_end = classified_verts.end();
	vector<BSP_VertexInd>::const_iterator v_it = classified_verts.begin();

	for (; v_it != v_end; ++v_it) {
		vertex_set[*v_it].SetOpenTag(e_unclassified);
	}

	// tidy up the cached arrays.

	m_spanning_edges.clear();
	m_visited_edges.clear();
	m_visited_verts.clear();


	// le fin.

}
		

	BSP_VertexInd 
BSP_CSGMeshSplitter::
SplitEdge(	
	BSP_CSGMesh & mesh,
	BSP_EdgeInd ei,
	BSP_MVertex &vertex,
	MT_Scalar epsilon
){
	vector<BSP_MEdge> & edge_set = mesh.EdgeSet();
	vector<BSP_MVertex> & vertex_set = mesh.VertexSet();
	vector<BSP_MFace> & face_set = mesh.FaceSet();
	
	MT_assert(edge_set.size() > (unsigned int)(ei));

	if (edge_set.size() <= (unsigned int)(ei)) {
		BSP_CSGException e(e_param_error);
		throw(e);
	}
	
	// push the vertex onto the vertex array
	BSP_VertexInd new_vi = vertex_set.size();
	vertex_set.push_back(vertex);
	BSP_MVertex & new_v = vertex_set[new_vi];

	// copy the edge because the new edge will have 
	// exactly the same face set.

	BSP_EdgeInd new_ei = edge_set.size();

	// Note never use set.push_back(set[i])
	// coz push_back excepts a reference which may become
	// invalid if the set is resized
	
	edge_set.push_back(BSP_MEdge());
	edge_set[new_ei] = edge_set[ei];
	BSP_MEdge & new_e = edge_set[new_ei];

	BSP_MEdge &e = edge_set[ei];

	// get the edge vertices.
	BSP_MVertex & e_v2 = vertex_set[e.m_verts[1]];

	// Remove the split edge from vertex 2.
	// Let's hope that the vertex isn't using this edge for
	// its' open tag!!

	BSP_Classification v2_class = BSP_Classification(e_v2.OpenTag());

	e_v2.RemoveEdge(ei);

	// add the split edge to the new vertex.
	new_v.AddEdge(ei);
		
	// add the new edge to the new vertex.
	new_v.AddEdge(new_ei);

	// add the new edge to vertex 2
	e_v2.AddEdge(new_ei);

	// Reset the tags for modified vertex.

	e_v2.SetOpenTag(v2_class);


	// Replace the old vertex indices in the new edge.
	new_e.m_verts[0] = new_vi;
	e.m_verts[1] = new_vi;

	// Finally add the vertex in the correct position to the
	// neighbouring polygons.
	
	vector<BSP_FaceInd>::iterator neighbour_face_it = e.m_faces.begin();
	vector<BSP_FaceInd>::const_iterator neighbour_face_end = e.m_faces.end();

	for (; neighbour_face_it != neighbour_face_end; ++neighbour_face_it) {

		mesh.InsertVertexIntoFace(
			face_set[*neighbour_face_it],
			new_e.m_verts[1],
			e.m_verts[0],
			new_vi,
			m_fv_func,
			epsilon
		);	
	}
	
	// That should be it (cough)
	return new_vi;

}

	void
BSP_CSGMeshSplitter::
SplitPolygon(
	BSP_CSGMesh & mesh,
	BSP_FaceInd fi,
	BSP_FaceInd &fin,
	BSP_FaceInd &fout
){
	vector<BSP_MEdge> & edge_set = mesh.EdgeSet();
	vector<BSP_MVertex> & vertex_set = mesh.VertexSet();
	vector<BSP_MFace> & face_set = mesh.FaceSet();

	MT_assert(face_set.size() > (unsigned int)(fi));
	if (face_set.size() <= (unsigned int)(fi)) {
		BSP_CSGException e(e_param_error);
		throw(e);
	}

	BSP_MFace & face = face_set[fi];

	// Walk throught the vertices of this polygon.
	// generate inside and outside loops.

	vector<BSP_VertexInd>::const_iterator f_verts_end = face.m_verts.end();
	vector<BSP_VertexInd>::iterator f_verts_it = face.m_verts.begin();
	
	vector<BSP_UserFVInd>::const_iterator f_fv_data_it = face.m_fv_data.begin();

	// NOTE we don't actually duplicate fv data for this face 
	// we just duplicate the indices, so both on vertices
	// will share the fv data.

	for (;f_verts_it != f_verts_end; ++f_verts_it, ++f_fv_data_it) {

		BSP_MVertex & vert = vertex_set[*f_verts_it];
		BSP_Classification v_class = BSP_Classification(vert.OpenTag());

		if (v_class == e_classified_in) {
			m_in_loop.push_back(*f_verts_it);
			m_fv_in_loop.push_back(*f_fv_data_it);
		} else 
		if (v_class == e_classified_out) {
			m_out_loop.push_back(*f_verts_it);
			m_fv_out_loop.push_back(*f_fv_data_it);

		} else
		if (v_class == e_classified_on) {
			m_in_loop.push_back(*f_verts_it);
			m_out_loop.push_back(*f_verts_it);
			m_on_loop.push_back(*f_verts_it);
			m_fv_in_loop.push_back(*f_fv_data_it);
			m_fv_out_loop.push_back(*f_fv_data_it);
		} else {
			// The vertex is unclassified this is an error!
			MT_assert(false);
			BSP_CSGException e(e_split_error);
			throw(e);
		}
	}
	
	if ((m_in_loop.size() == 1) || (m_out_loop.size() == 1)) {
		// Then there was only 1 tagged vertex I guess this is fine
		// but should be reviewed!

		// NOT fine - we only ever split spanning polygons.
		MT_assert(false);
		BSP_CSGException e(e_split_error);
		throw(e);
	}

	MT_assert(m_in_loop.size() >=3 && m_out_loop.size() >=3 && m_on_loop.size() == 2);

	if (m_in_loop.size() <3 || m_out_loop.size() <3 || m_on_loop.size() !=2) {
		BSP_CSGException e(e_split_error);
		throw(e);
	}		
	// Now we have 2 seperate vertex loops representing the polygon 
	// halves.
	
	// create a new polygon for the out_loop of vertices.
	////////////////////////////////////////////////////

	// Duplicate face data.

	mesh.FaceData().Duplicate(fi);

	BSP_FaceInd new_fi = face_set.size();
	face_set.push_back(BSP_MFace());
	BSP_MFace & new_f = face_set[new_fi];

	// assign plane equation for new face - this is the same as the 
	// original of course.
	
	new_f.m_plane = face_set[fi].m_plane;

	// note that face may have become invalid because we just been adding
	// more polygons to the array. We can't assign a new reference to the old
	// invlaid one! It will call try and call the assignment operator on
	// the original face!!!! The joys of references! We just use face_set[fi]
	// from now on to be safe

	// We need to insert an edge between m_on_loop[0] and m_on_loop[1] 
	
	// Make sure the edge does not already exist between these 2 vertices!
	// This can happen if the original mesh has duplicate polygons.
	// We still wire up the new polygons to this edge, which will 
	// lead to duplicate polygons in the output -- but algorithm
	// should still work.
	BSP_EdgeInd new_ei = mesh.FindEdge(m_on_loop[0],m_on_loop[1]);

	if (new_ei.IsEmpty()) {

		// create a new edge.

		new_ei = edge_set.size();
		edge_set.push_back(BSP_MEdge());
		BSP_MEdge & new_e = edge_set[new_ei];

		new_e.m_verts[0] = m_on_loop[0];
		new_e.m_verts[1] = m_on_loop[1];

		// Now tie the edge to it's vertices.
		
		vertex_set[m_on_loop[0]].AddEdge(new_ei);
		vertex_set[m_on_loop[1]].AddEdge(new_ei);
	}

	edge_set[new_ei].m_faces.push_back(fi);
	// This next line is a trick we are going to replace it in a moment
	// with new_fi. It means that all the edges of the new polygon have
	// pointers to the old polygon which we can replace.
	edge_set[new_ei].m_faces.push_back(fi); 
		
 
	// Replace the old polygons vertex loop with the in_loop of vertices.
	
	face_set[fi].m_verts = m_in_loop;
	new_f.m_verts = m_out_loop;

	// Replace the olf fv loops.
	face_set[fi].m_fv_data = m_fv_in_loop;
	new_f.m_fv_data = m_fv_out_loop;


	// That should be it for the old polygon;
	// For the new polygon we just need to iterate around it's 
	// edges and replace pointers to the old polygon with pointers
	// to the new one.

	f_verts_end = new_f.m_verts.end();
	f_verts_it = new_f.m_verts.begin();

	BSP_VertexInd prev = new_f.m_verts.back();

	for (;f_verts_it != f_verts_end; ++f_verts_it) {
		BSP_EdgeInd new_f_ei = mesh.FindEdge(prev,*f_verts_it);
		
		MT_assert(!new_f_ei.IsEmpty());

		if (new_f_ei.IsEmpty()) {
			BSP_CSGException e(e_split_error);
			throw(e);
		}
	
		edge_set[new_f_ei].SwapFace(fi,new_fi);	
		prev = *f_verts_it;

	}
	
	// That should be everything.

	fin = fi;
	fout = new_fi;

	// clear up cached helpers.
	m_in_loop.clear();
	m_on_loop.clear();
	m_out_loop.clear();

	m_fv_in_loop.clear();
	m_fv_out_loop.clear();

}

	BSP_FaceInd
BSP_CSGMeshSplitter::
TriangulateConvexQuad(
	BSP_CSGMesh & mesh,
	const BSP_FaceInd fi
){

	// we assume that the fi points to a face with 
	// exactly 4 vertices.


	// We are definately going to create a new face 
	// so lets make space for it in the face array.

	vector<BSP_MFace> & face_set = mesh.FaceSet();
	vector<BSP_MVertex> & vertex_set = mesh.VertexSet();
	vector<BSP_MEdge> & edge_set = mesh.EdgeSet();

	if (face_set[fi].m_verts.size() == 3) {
		return BSP_FaceInd::Empty();
	}
		
	// duplicate face data
	mesh.FaceData().Duplicate(fi);

	const BSP_FaceInd new_fi = face_set.size();
	face_set.push_back(BSP_MFace());
	BSP_MFace & new_face = face_set[new_fi];
	BSP_MFace & face = face_set[fi];

	new_face.m_plane = face.m_plane;

	// The internal edges are [0,2] and [1,3]
	// these split the quad into the triangles 
	// [0,1,2],[2,3,0] and [0,1,3],[1,2,3] respectively
	
	const MT_Point3 & p0 = vertex_set[face.m_verts[0]].m_pos;
	const MT_Point3 & p1 = vertex_set[face.m_verts[1]].m_pos;
	const MT_Point3 & p2 = vertex_set[face.m_verts[2]].m_pos;
	const MT_Point3 & p3 = vertex_set[face.m_verts[3]].m_pos;
 
	MT_Vector3 e0 = p1 - p0;
	MT_Vector3 e1 = p2 - p1;
	MT_Vector3 e2 = p3 - p2;
	MT_Vector3 e3 = p0 - p3;

	MT_Scalar A = (e0.cross(e1)).length2();
	MT_Scalar B = (e2.cross(e3)).length2();
	MT_Scalar C = (e3.cross(e0)).length2();
	MT_Scalar D = (e1.cross(e2)).length2();

	MT_Scalar minab = MT_min(A,B);
	MT_Scalar maxab = MT_max(A,B);

	MT_Scalar mincd = MT_min(C,D);
	MT_Scalar maxcd = MT_max(C,D);
	
	MT_Scalar ratioab = minab/maxab;
	MT_Scalar ratiocd = mincd/maxcd;
	
	ratioab = MT_abs(1-ratioab);
	ratiocd = MT_abs(1-ratiocd);

	vector<BSP_VertexInd> loop1(3),loop2(3);
	vector<BSP_UserFVInd> fv_loop1(3),fv_loop2(3);

	if (ratioab < ratiocd) {
		// then use [0,2] as splitting edge.
		loop1[0] = face.m_verts[1];
		loop1[1] = face.m_verts[2];
		loop1[2] = face.m_verts[0];
	
		loop2[0] = face.m_verts[2];
		loop2[1] = face.m_verts[3];
		loop2[2] = face.m_verts[0];
	
		// same for fv indices.
		fv_loop1[0] = face.m_fv_data[1];
		fv_loop1[1] = face.m_fv_data[2];
		fv_loop1[2] = face.m_fv_data[0];

		fv_loop2[0] = face.m_fv_data[2];
		fv_loop2[1] = face.m_fv_data[3];
		fv_loop2[2] = face.m_fv_data[0];
			

	} else {
		// use [1,3] as splitting edge
		loop1[0] = face.m_verts[0];
		loop1[1] = face.m_verts[1];
		loop1[2] = face.m_verts[3];
	
		loop2[0] = face.m_verts[1];
		loop2[1] = face.m_verts[2];
		loop2[2] = face.m_verts[3];

		// same for fv indices.
		fv_loop1[0] = face.m_fv_data[0];
		fv_loop1[1] = face.m_fv_data[1];
		fv_loop1[2] = face.m_fv_data[3];

		fv_loop2[0] = face.m_fv_data[1];
		fv_loop2[1] = face.m_fv_data[2];
		fv_loop2[2] = face.m_fv_data[3];

	}

	// TODO factor out commmon code between here and SplitPolygon.

	BSP_EdgeInd new_ei = mesh.FindEdge(loop1[1],loop1[2]);

	if (new_ei.IsEmpty()) {

		// create a new edge.

		new_ei = edge_set.size();
		edge_set.push_back(BSP_MEdge());
		BSP_MEdge & new_e = edge_set[new_ei];

		new_e.m_verts[0] = loop1[1];
		new_e.m_verts[1] = loop1[2];

		// Now tie the edge to it's vertices.
		
		vertex_set[loop1[1]].AddEdge(new_ei);
		vertex_set[loop1[2]].AddEdge(new_ei);
	}

	edge_set[new_ei].m_faces.push_back(fi);
	// This next line is a trick we are going to replace it in a moment
	// with new_fi. It means that all the edges of the new polygon have
	// pointers to the old polygon which we can replace.
	edge_set[new_ei].m_faces.push_back(fi); 
		
 
	// Replace the old polygons vertex loop with the in_loop of vertices.
	
	face.m_verts = loop1;
	face.m_fv_data = fv_loop1;
	new_face.m_verts = loop2;
	new_face.m_fv_data = fv_loop2;

	// That should be it for the old polygon;
	// For the new polygon we just need to iterate around it's 
	// edges and replace pointers to the old polygon with pointers
	// to the new one.

	vector<BSP_VertexInd>::const_iterator f_verts_end = new_face.m_verts.end();
	vector<BSP_VertexInd>::const_iterator f_verts_it = new_face.m_verts.begin();

	BSP_VertexInd prev = new_face.m_verts.back();

	for (;f_verts_it != f_verts_end; ++f_verts_it) {
		BSP_EdgeInd new_f_ei = mesh.FindEdge(prev,*f_verts_it);
		
		MT_assert(!new_f_ei.IsEmpty());

		if (new_f_ei.IsEmpty()) {
			BSP_CSGException e(e_split_error);
			throw(e);
		}
	
		edge_set[new_f_ei].SwapFace(fi,new_fi);	
		prev = *f_verts_it;

	}
	return new_fi;
}
