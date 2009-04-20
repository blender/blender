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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LOD_EdgeCollapser.h"

#include "LOD_ManMesh2.h"
#include "CTR_TaggedSetOps.h"
#include <algorithm>
#include <functional>


using namespace std;


	LOD_EdgeCollapser * 
LOD_EdgeCollapser::
New(
){
	return new LOD_EdgeCollapser();
}


	bool
LOD_EdgeCollapser::
TJunctionTest(
	LOD_ManMesh2 &mesh,
	vector<LOD_EdgeInd> &e_v0v1,
	LOD_EdgeInd collapse_edge
){

	// we need to copy the egdes in e_v0v1 from the mesh
	// into a new buffer -> we are going to modify them

	int original_size = e_v0v1.size();
	if (original_size == 0) return true;

	vector<LOD_Edge> &edge_set = mesh.EdgeSet();

	LOD_VertexInd c_v0 = edge_set[collapse_edge].m_verts[0];
	LOD_VertexInd c_v1 = edge_set[collapse_edge].m_verts[1];

	vector<LOD_Edge> temp_edges;
	temp_edges.reserve(e_v0v1.size());

	vector<LOD_EdgeInd>::iterator edge_it = e_v0v1.begin();
	vector<LOD_EdgeInd>::const_iterator edge_end = e_v0v1.end();

	for (;edge_it != edge_end; ++edge_it) {
		temp_edges.push_back(edge_set[*edge_it]);
	}

	// in the copied edges replace all instances of c_v0 with c_v1

	vector<LOD_Edge>::iterator e_it = temp_edges.begin();
	vector<LOD_Edge>::const_iterator e_it_end = temp_edges.end();
		
	for (; e_it != e_it_end; ++e_it) {

		if (e_it->m_verts[0] == c_v0) {
			e_it->m_verts[0] = c_v1;
		}
		if (e_it->m_verts[1] == c_v0) {
			e_it->m_verts[1] = c_v1;
		}

		// normalize the edge
		if (int(e_it->m_verts[0]) > int(e_it->m_verts[1])) {
			LOD_EdgeInd temp = e_it->m_verts[0];
			e_it->m_verts[0] = e_it->m_verts[1];
			e_it->m_verts[1] = temp;
		}
	}

	// sort the edges using the edge less functional 

	sort(temp_edges.begin(),temp_edges.end(),LOD_EdgeCollapser::less());
	// count the unique edges.

	e_it = temp_edges.begin();
	e_it_end = temp_edges.end();
	
	int coincedent_edges = 0;

	vector<LOD_Edge>::const_iterator last_edge = e_it;
	++e_it;
		
	for (; e_it != e_it_end; ++e_it) {
	
		if ((e_it->m_verts[0] == last_edge->m_verts[0]) &&
			(e_it->m_verts[1] == last_edge->m_verts[1])
		) {
			++coincedent_edges;
		}
		last_edge = e_it;
	}

	// now if the collapse edge is a boundary edges 
	// then we are alloved at most one coincedent edge

	// otherwise at most 2 coincedent edges

	if (edge_set[collapse_edge].BoundaryEdge()) {
		return (coincedent_edges > 1);
	} else {
		return (coincedent_edges > 2);
	}


}


	
	bool
LOD_EdgeCollapser::
CollapseEdge(
	LOD_EdgeInd ei,
	LOD_ManMesh2 &mesh,
	vector<LOD_EdgeInd> & degenerate_edges,
	vector<LOD_FaceInd> & degenerate_faces,
	vector<LOD_VertexInd> & degenerate_vertices,
	vector<LOD_EdgeInd> & new_edges,
	vector<LOD_FaceInd> & update_faces,
	vector<LOD_VertexInd> & update_vertices
){

	vector<LOD_Vertex>	&verts = mesh.VertexSet();
	vector<LOD_Edge>	&edges = mesh.EdgeSet();
	vector<LOD_TriFace> &faces = mesh.FaceSet();

	// shouldn't do this (use mesh interface instead!)
	LOD_VertexInd v0_ind = edges[ei].m_verts[0];
	LOD_VertexInd v1_ind = edges[ei].m_verts[1];
#if 0
	LOD_Vertex &v0 = verts[v0_ind];
	LOD_Vertex &v1 = verts[v1_ind];
#endif
	vector<vector<LOD_EdgeInd> > e_v01(2);
	e_v01[0].reserve(32);
	e_v01[1].reserve(32);
	
	mesh.VertexEdges(v0_ind,e_v01[0]);
	mesh.VertexEdges(v1_ind,e_v01[1]);


	// compute the union of e_v0 and e_v1 -> this is the degenerate edges of the collapse
	// we remove old edges and replace edges inside the collapse zone with new ones 

	CTR_TaggedSetOps<LOD_EdgeInd,LOD_Edge>::Union(e_v01,edges,degenerate_edges);

	vector< vector<LOD_FaceInd> > p_v01(2);
	p_v01[0].reserve(32);
	p_v01[1].reserve(32);

	mesh.VertexFaces(v0_ind,p_v01[0]);
	mesh.VertexFaces(v1_ind,p_v01[1]);

	// compute the union of p_v0 anf p_v1
	vector<LOD_FaceInd> p_v0v1;
	p_v0v1.reserve(32);

	CTR_TaggedSetOps<LOD_FaceInd,LOD_TriFace>::Union(p_v01,faces,p_v0v1);

	// compute the union of all the edges in p_v0v1 this is the collapse zone

	vector<vector<LOD_EdgeInd> > e_input_vectors(p_v0v1.size());

	vector<LOD_FaceInd>::iterator p_v0v1_end = p_v0v1.end();
	vector<LOD_FaceInd>::iterator p_v0v1_start = p_v0v1.begin();

	vector<vector<LOD_FaceInd> >::iterator vector_insert_it = e_input_vectors.begin();
	
	for (;p_v0v1_start != p_v0v1_end; ++p_v0v1_start , ++vector_insert_it) {
		mesh.FaceEdges(*p_v0v1_start,*vector_insert_it);
	}

	vector<LOD_EdgeInd> collapse_zone;
	collapse_zone.reserve(32);

	CTR_TaggedSetOps<LOD_EdgeInd,LOD_Edge>::Union(e_input_vectors,edges,collapse_zone);

	// compute the ring edges = collpase_zone - e_v0v1

	vector<LOD_EdgeInd> edge_ring;
	edge_ring.reserve(32);

	CTR_TaggedSetOps<LOD_EdgeInd,LOD_Edge>::Difference(collapse_zone,degenerate_edges,edges,edge_ring);

	// T Junction test
	//////////////////
	// At this point we check to see if any of the polygons
	// in p_v0v1 are coninceddent - this leads
	// to errors later on if we try and insert a polygon
	// into the mesh to an edge which already has 2 polygons.

	// not that t junctions occur naturally from edge collapses
	// and are not just the result of coincedent polygons
	// for example consider collapsing an edge that forms part
	// of a triangular bottle neck.

	// Really we need to make sure that we don't create t-junctions.

	// I think that a sufficient test is to check the number of
	// coincedent edge pairs after a collapse. If it is more than 2
	// then collapsing the edge may result in an undeleted edge 
	// sharing more than 2 polygons. This test probably is too 
	// restictive though.
	
	// To perform this test we need to make a copy of the edges
	// in e_v0v1. We then apply the contraction to these edge
	// copies. Sort them using a function that places coincedent 
	// edges next to each other. And then count the number
	// of coincedent pairs. 

	// Of course we have to do this test before we change any of the
	// mesh -> so we can back out safely.

	if (TJunctionTest(mesh,degenerate_edges,ei)) return false; 

	// Compute the set of possibly degenerate vertices
	// this is the union of all the vertices of polygons
	// of v0 and v1

	vector<LOD_FaceInd>::iterator face_it = p_v0v1.begin();
	vector<LOD_FaceInd>::const_iterator face_end = p_v0v1.end();


	vector<vector<LOD_VertexInd> > p_v0v1_vertices(p_v0v1.size());
	
	for (int i = 0; face_it != face_end; ++face_it, ++i) {
		mesh.FaceVertices(*face_it,p_v0v1_vertices[i]);
	}
	
	vector<LOD_VertexInd> vertex_ring;
	vertex_ring.reserve(32);

	CTR_TaggedSetOps<LOD_VertexInd,LOD_Vertex>::Union(p_v0v1_vertices,verts,vertex_ring);

	// remove all the internal edges e_v0v1 from the mesh.
	// for each edge remove the egde from it's vertices edge lists.

	vector<LOD_EdgeInd>::iterator edge_it = degenerate_edges.begin();
	vector<LOD_EdgeInd>::const_iterator edge_end = degenerate_edges.end();

	for (; !(edge_it == edge_end); ++edge_it) {
			
		LOD_EdgeInd ed = (*edge_it);
		LOD_Edge & edge = edges[ed];//*edge_it];
	
		verts[edge.m_verts[0]].RemoveEdge(ed);
		verts[edge.m_verts[1]].RemoveEdge(ed);
	}

	// we postpone deletion of the internal edges untill the end
	// this is because deleting edges invalidates all of the 
	// EdgeInd vectors above.

		
	// now untie all the polygons in p_v0v1 from the edge ring

	// select all polygons in p_v0v1

	face_it = p_v0v1.begin();
	face_end = p_v0v1.end();

	for (;face_it != face_end; ++face_it) {
		faces[*face_it].SetSelectTag(true);
	}

	edge_it = edge_ring.begin();
	edge_end = edge_ring.end();

	for (;edge_it != edge_end; ++edge_it) {
		LOD_Edge & edge = edges[*edge_it];

		// presumably all edges in edge_ring point to at least
		// one polygon from p_v0v1
		
		if (!edge.m_faces[0].IsEmpty() && faces[edge.m_faces[0]].SelectTag()) {
			edge.m_faces[0].Invalidate();
		}

		if (!edge.m_faces[1].IsEmpty() && faces[edge.m_faces[1]].SelectTag()) {
			edge.m_faces[1].Invalidate();
		}
	}
	
	// deselect the faces

	face_it = p_v0v1.begin();
	face_end = p_v0v1.end();

	for (;face_it != face_end; ++face_it) {
		faces[*face_it].SetSelectTag(false);
	}

	// perform the edge collapse
	////////////////////////////

	// iterate through the polygons of p_v0 and replace the vertex
	// index v0 with v1

	face_it = p_v01[0].begin();
	face_end = p_v01[0].end();
	
	for (;face_it != face_end; ++face_it) {
		faces[*face_it].SwapVertex(v0_ind,v1_ind);
	}

	face_it = p_v0v1.begin();
	face_end = p_v0v1.end();
	
	for (;face_it != face_end; ++face_it) {
		if (faces[*face_it].Degenerate()) {
			degenerate_faces.push_back(*face_it);
		} else {
			update_faces.push_back(*face_it);
		}
	}
	
	// Add all the non-degenerate faces back into the 
	// mesh. Get a record of the new edges created in
	// this process.

	face_it = update_faces.begin();
	face_end = update_faces.end();

	for (;face_it != face_end; ++face_it) {
		mesh.ConnectTriangle(*face_it,new_edges);
	}

	// degenerate ring primitives
	/////////////////////////////

	// we now need to examine each of the edges on the ring
	// and work out if they are degenerate - if so we attempt
	// to delete them -> add them to the other edges to delete
	// in e_v0v1

	edge_it = edge_ring.begin();
	edge_end = edge_ring.end();

	for (;edge_it != edge_end; ++edge_it) {
		if (edges[*edge_it].Degenerate()) {
			degenerate_edges.push_back(*edge_it);
		}
	}

	// do the same for the ring vertices.
		
	vector<LOD_VertexInd>::iterator vertex_it = vertex_ring.begin();
	vector<LOD_VertexInd>::const_iterator vertex_end = vertex_ring.end();

	for (;vertex_it != vertex_end; ++vertex_it) {
		if (verts[*vertex_it].Degenerate()) {
			degenerate_vertices.push_back(*vertex_it);
		} else {
			update_vertices.push_back(*vertex_it);
		}
	}

	// we now know all the degenerate primitives
	// and the new primitives we have inserted into the mesh

	// We now delete the mesh primitives, mesh.DeleteXXXXXX() methods
	// assume that the index vectors are sorted into descending order.
	// we do that now.

	sort(degenerate_edges.begin(),degenerate_edges.end(),LOD_EdgeInd::greater());
	sort(degenerate_faces.begin(),degenerate_faces.end(),LOD_FaceInd::greater());
	sort(degenerate_vertices.begin(),degenerate_vertices.end(),LOD_VertexInd::greater());

	
	return true;
	
}

LOD_EdgeCollapser::
LOD_EdgeCollapser(
){
	// nothing to do
}
