/**
 * $Id$
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

#include "LOD_ManMesh2.h"

#include "MT_assert.h"
#include <algorithm>
#include "LOD_MeshException.h"
#include "CTR_TaggedSetOps.h"
#include "CTR_UHeap.h"
#include "LOD_ExternBufferEditor.h"


using namespace std;

LOD_ManMesh2::
LOD_ManMesh2(
) :
	m_bbox_min(0,0,0),
	m_bbox_max(0,0,0)
{
}
	

	LOD_ManMesh2 *
LOD_ManMesh2::
New(
){
	MEM_SmartPtr<LOD_ManMesh2> output(new LOD_ManMesh2());
	if (output == NULL) return NULL;

	// build the vertex, edge and face sets.

	MEM_SmartPtr<vector<LOD_Vertex> > verts(new vector<LOD_Vertex>);
	MEM_SmartPtr<vector<LOD_TriFace> > faces(new vector<LOD_TriFace>);
	MEM_SmartPtr<vector<LOD_Edge> > edges(new vector<LOD_Edge>);
	
	if ((faces == NULL) || (edges == NULL) || (verts == NULL)) {
		return NULL;
	}
	
	output->m_verts = verts.Release();
	output->m_faces = faces.Release();
	output->m_edges = edges.Release();

	return output.Release();
}	

// take ownership of the vertices.

	bool	
LOD_ManMesh2::
SetVertices(
	MEM_SmartPtr<vector<LOD_Vertex> > verts
){


	// take ownership of vertices
	m_verts = verts;

	// create a polygon and edge buffer of half the size 
	// and just use the automatic resizing feature of vector<>
	// to worry about the dynamic array resizing
	
	m_faces->clear();
	m_edges->clear();

	m_faces->reserve(m_verts->size()/2);
	m_edges->reserve(m_verts->size()/2);

	return true;	

}

	
// add a triangle to the mesh

	void
LOD_ManMesh2::
AddTriangle(
	int verts[3]
) {

	MT_assert(verts[0] < int(m_verts->size()));
	MT_assert(verts[1] < int(m_verts->size()));
	MT_assert(verts[2] < int(m_verts->size()));

	LOD_TriFace face;
	face.m_verts[0] = verts[0];
	face.m_verts[1] = verts[1];
	face.m_verts[2] = verts[2];

	LOD_FaceInd face_index = m_faces->size();

	m_faces->push_back(face);	

	// now work out if any of the directed edges or their
	// companion edges exist already.
	// We go through the edges associated with each of the given vertices 

	// the safest thing to do is iterate through each of the edge sets
	// check against each of the 2 other triangle edges to see if they are there
	
	vector<LOD_EdgeInd> new_edges;
	new_edges.reserve(3);

	InsertEdge(verts[0],verts[1],face_index,new_edges);
	InsertEdge(verts[1],verts[2],face_index,new_edges);
	InsertEdge(verts[2],verts[0],face_index,new_edges);

}
	
// Adds the index of any created edges to new_edges

	bool
LOD_ManMesh2::
InsertEdge(
	const LOD_VertexInd v1,
	const LOD_VertexInd v2,
	const LOD_FaceInd f,
	vector<LOD_EdgeInd> &new_edges
){
	
	MT_assert(!v1.IsEmpty());
	MT_assert(!v2.IsEmpty());
	MT_assert(!f.IsEmpty());

	vector<LOD_Vertex> &verts = VertexSet();
	vector<LOD_Edge> &edges = EdgeSet();
	
	LOD_EdgeInd e;

	e = FindEdge(v1,v2);

	if (e.IsEmpty()) {
		// This edge does not exist -- make a new one 

		LOD_Edge temp_e;
		temp_e.m_verts[0] = v1;
		temp_e.m_verts[1] = v2;

		e = m_edges->size();

		// set the face ptr for this half-edge
		temp_e.m_faces[0] = f;

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

		LOD_Edge &edge = edges[e];

		edge.SwapFace(LOD_FaceInd::Empty(),f);
	}
		

	return true;

}

	void
LOD_ManMesh2::
ConnectTriangle(
	LOD_FaceInd fi,
	std::vector<LOD_EdgeInd> & new_edges
){

	vector<LOD_TriFace> &faces = FaceSet();

	MT_assert(!faces[fi].Degenerate());

	LOD_TriFace & face = faces[fi];

	InsertEdge(face.m_verts[0],face.m_verts[1],fi,new_edges);
	InsertEdge(face.m_verts[1],face.m_verts[2],fi,new_edges);
	InsertEdge(face.m_verts[2],face.m_verts[0],fi,new_edges);
};




// geometry access
//////////////////

	vector<LOD_Vertex> &
LOD_ManMesh2::
VertexSet(
) const {
	return m_verts.Ref();
}		

	vector<LOD_TriFace> &
LOD_ManMesh2::
FaceSet(
) const {
	return m_faces.Ref();
}

	vector<LOD_Edge> &
LOD_ManMesh2::
EdgeSet(
) const {
	return m_edges.Ref();
};

LOD_ManMesh2::
~LOD_ManMesh2(
){
	//auto ptr takes care of vertex arrays etc.
}

	LOD_EdgeInd
LOD_ManMesh2::
FindEdge(
	const LOD_VertexInd v1,
	const LOD_VertexInd v2
) {

	vector<LOD_Vertex> &verts = VertexSet();
	vector<LOD_Edge> &edges = EdgeSet();

	LOD_Edge e;
	e.m_verts[0] = v1;
	e.m_verts[1] = v2;
	
	vector<LOD_EdgeInd> &v1_edges = verts[v1].m_edges;
	vector<LOD_EdgeInd>::const_iterator v1_end = v1_edges.end();
	vector<LOD_EdgeInd>::iterator v1_begin = v1_edges.begin();

	for (; v1_begin != v1_end; ++v1_begin) {
		if (edges[*v1_begin] == e) return *v1_begin;
	}
	
	return LOD_EdgeInd::Empty();
}

// face queries
///////////////

	void
LOD_ManMesh2::
FaceVertices(
	LOD_FaceInd fi,
	vector<LOD_VertexInd> &output
){	
	const vector<LOD_TriFace> &faces = FaceSet();
	const LOD_TriFace & f = faces[fi];

	output.push_back(f.m_verts[0]);
	output.push_back(f.m_verts[1]);
	output.push_back(f.m_verts[2]);
}
	
	void
LOD_ManMesh2::
FaceEdges(
	LOD_FaceInd fi,
	vector<LOD_EdgeInd> &output
){
	const vector<LOD_TriFace> &faces = FaceSet();
	vector<LOD_Edge> &edges = EdgeSet();
	vector<LOD_Vertex> &verts = VertexSet();	

	const LOD_TriFace & f = faces[fi];
	// intersect vertex edges

	vector<LOD_EdgeInd> & v0_edges = verts[f.m_verts[0]].m_edges;
	vector<LOD_EdgeInd> & v1_edges = verts[f.m_verts[1]].m_edges;
	vector<LOD_EdgeInd> & v2_edges = verts[f.m_verts[2]].m_edges;

	CTR_TaggedSetOps<LOD_EdgeInd,LOD_Edge>::IntersectPair(v0_edges,v1_edges,edges,output);
	CTR_TaggedSetOps<LOD_EdgeInd,LOD_Edge>::IntersectPair(v1_edges,v2_edges,edges,output);
	CTR_TaggedSetOps<LOD_EdgeInd,LOD_Edge>::IntersectPair(v2_edges,v0_edges,edges,output);

	MT_assert(output.size() == 3);
	if (output.size() != 3) {
		LOD_MeshException e(LOD_MeshException::e_non_manifold);
		throw(e);	
	}
}
	

// edge queries
///////////////

	void
LOD_ManMesh2::
EdgeVertices(
	LOD_EdgeInd ei,
	vector<LOD_VertexInd> &output
){
	const vector<LOD_Edge> &edges = EdgeSet();
	const LOD_Edge & e = edges[ei];

	output.push_back(e.m_verts[0]);
	output.push_back(e.m_verts[1]);
}

	void
LOD_ManMesh2::
EdgeFaces(
	LOD_EdgeInd ei,
	vector<LOD_FaceInd> &output
){
	const vector<LOD_Edge> &edges = EdgeSet();
	const LOD_Edge & e = edges[ei];

	if (!e.m_faces[0].IsEmpty()) {
		output.push_back(e.m_faces[0]);
	}
	if (!e.m_faces[1].IsEmpty()) {
		output.push_back(e.m_faces[1]);
	}
}	

// vertex queries
/////////////////

	void
LOD_ManMesh2::
VertexEdges(
	LOD_VertexInd vi,
	vector<LOD_EdgeInd> &output
){
	// iterate through the edges of v and push them onto the 
	// output
	
	vector<LOD_Vertex> &verts = VertexSet();

	vector<LOD_EdgeInd> & v_edges = verts[vi].m_edges;
	vector<LOD_EdgeInd>::iterator v_it = v_edges.begin();

	for (; v_it != v_edges.end(); ++v_it) {
		output.push_back(*v_it);
	}	
}

	void
LOD_ManMesh2::
VertexFaces(
	LOD_VertexInd vi,
	vector<LOD_FaceInd> &output
){
	const vector<LOD_Vertex> &verts = VertexSet();
	vector<LOD_Edge> &edges = EdgeSet();
	vector<LOD_TriFace> &faces = FaceSet();

	const vector<LOD_EdgeInd> &v_edges = verts[vi].m_edges;
	vector<LOD_EdgeInd>::const_iterator e_it = v_edges.begin();

	for (; e_it != v_edges.end(); ++e_it) {

		LOD_Edge &e = edges[*e_it]; 

		if ((!e.m_faces[0].IsEmpty()) && (!faces[e.m_faces[0]].SelectTag())) {
			output.push_back(e.m_faces[0]);
			faces[e.m_faces[0]].SetSelectTag(true);
		}

		if ((!e.m_faces[1].IsEmpty()) && (!faces[e.m_faces[1]].SelectTag())) {
			output.push_back(e.m_faces[1]);
			faces[e.m_faces[1]].SetSelectTag(true);
		}
	}

	vector<LOD_FaceInd>::iterator f_it = output.begin();

	for (; f_it != output.end(); ++f_it) {
		faces[*f_it].SetSelectTag(false);
	}
};
		
	void
LOD_ManMesh2::
SetBBox(
	MT_Vector3 bbox_min,
	MT_Vector3 bbox_max
){
	m_bbox_min = bbox_min;
	m_bbox_max = bbox_max;
};

	void
LOD_ManMesh2::
SC_TriFace(
	LOD_FaceInd f
){
	LOD_TriFace face = (*m_faces)[f];
	
	// check for unique vertices

	if (
		(face.m_verts[0] == face.m_verts[1]) ||
		(face.m_verts[1] == face.m_verts[2]) ||
		(face.m_verts[2] == face.m_verts[0])
	) {
		MT_assert(false);
	}	

}


	void
LOD_ManMesh2::
SC_EdgeList(
	LOD_VertexInd v
){
	vector<LOD_Edge> &edges = EdgeSet();
	vector<LOD_Vertex> &verts = VertexSet();

	vector<LOD_EdgeInd>::iterator e_it = verts[v].m_edges.begin();

	for (;e_it != verts[v].m_edges.end(); ++e_it) {
		MT_assert( (edges[*e_it].m_verts[0] == v) || (edges[*e_it].m_verts[1] == v));
	}				

};
	
	void
LOD_ManMesh2::
DeleteVertex(
	LOD_ExternBufferEditor & extern_editor,
	LOD_VertexInd v
){

	vector<LOD_Edge> &edges = EdgeSet();
	vector<LOD_Vertex> &verts = VertexSet();
	vector<LOD_TriFace> &faces = FaceSet();

	// need to update all the edge and polygons pointing to 
	// the last vertex in m_verts

	if (verts.size() == 1) {
		verts.clear();
		return;
	}

	LOD_VertexInd last = LOD_VertexInd(size_t(verts.end() - verts.begin() - 1));

	if (!(last == v)) {

		// we asume that v is already disconnected

		vector<LOD_FaceInd> v_faces;
		vector<LOD_EdgeInd> v_edges;

		v_faces.reserve(64);
		v_edges.reserve(64);

		VertexFaces(last,v_faces);
		VertexEdges(last,v_edges);

		// map the faces and edges to look at v	

		vector<LOD_FaceInd>::iterator face_it = v_faces.begin();

		for(; face_it != v_faces.end(); ++face_it) {
			faces[*face_it].SwapVertex(last,v);
		}
		vector<LOD_EdgeInd>::iterator edge_it = v_edges.begin();

		for (; edge_it != v_edges.end(); ++edge_it) {
			edges[*edge_it].SwapVertex(last,v);
		}

		// copy the last vertex onto v and pop off the back.

		verts[v] = verts[last];

		// tidy external buffer
		extern_editor.CopyModifiedFaces(*this,v_faces);
	}

	verts.pop_back();	
	extern_editor.CopyBackVertex(v);


};		

	void
LOD_ManMesh2::
DeleteEdge(
	LOD_EdgeInd e,
	CTR_UHeap<LOD_Edge> * heap
){
	vector<LOD_Edge> &edges = EdgeSet();
	vector<LOD_Vertex> &verts = VertexSet();

	if (edges.size() == 1) {
		edges.clear();
		return;
	}

	LOD_EdgeInd last = LOD_EdgeInd(size_t(edges.end() - edges.begin() - 1));

	if (!(last == e)) {
		vector<LOD_EdgeInd> e_verts;
		e_verts.reserve(2);
		EdgeVertices(last,e_verts);
		// something is wrong if there arent two!

		verts[e_verts[0]].SwapEdge(last,e);
		verts[e_verts[1]].SwapEdge(last,e);

		// edges[e] should already have been removed from the heap

		MT_assert(edges[e].HeapPos() == -1);

		edges[e] = edges[last];
		// also have to swap there heap positions.!!!!!

		heap->HeapVector()[edges[e].HeapPos()] = e;


	}
	edges.pop_back();
};
	
	void
LOD_ManMesh2::
DeleteFace(
	LOD_ExternBufferEditor & extern_editor,
	LOD_FaceInd f
){

	vector<LOD_Edge> &edges = EdgeSet();
	vector<LOD_TriFace> &faces = FaceSet();

	if (faces.size() == 1) {
		faces.clear();
		return;
	}

	LOD_FaceInd last = LOD_FaceInd(size_t (faces.end() - faces.begin() - 1));

	if (!(last == f)) {
		
		// we have to update the edges which point to the last 
		// face 

		vector<LOD_EdgeInd> f_edges;
		f_edges.reserve(3);

		FaceEdges(last,f_edges);

		vector<LOD_EdgeInd>::iterator edge_it = f_edges.begin();
		vector<LOD_EdgeInd>::const_iterator edge_end = f_edges.end();
	
		for (; edge_it != edge_end; ++edge_it) {
			edges[*edge_it].SwapFace(last,f);
		}

		faces[f] = faces[last];

	}
	faces.pop_back();

	// tidy external buffers
	extern_editor.CopyBackFace(f);
};	
















