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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LOD_QSDecimator.h"

#include "LOD_ExternBufferEditor.h"

using namespace std;

	LOD_QSDecimator *
LOD_QSDecimator::
New(
	LOD_ManMesh2 &mesh,
	LOD_ExternNormalEditor &face_editor,
	LOD_ExternBufferEditor &extern_editor
){

	MEM_SmartPtr<LOD_QSDecimator> output 
		= new LOD_QSDecimator(mesh,face_editor,extern_editor);

	MEM_SmartPtr<LOD_EdgeCollapser > collapser(LOD_EdgeCollapser::New());
	MEM_SmartPtr<LOD_QuadricEditor> q_editor(LOD_QuadricEditor::New(mesh));

	if (
		output == NULL ||
		collapser == NULL ||
		q_editor == NULL 
	) {
		return NULL;
	}
	output->m_collapser = collapser.Release();
	output->m_quadric_editor = q_editor.Release();
	return output.Release();
}	



	bool
LOD_QSDecimator::
Arm(
){
	MT_assert(!m_is_armed);
	bool heap_result = BuildHeap();
	if (!heap_result) {
		return false;
	}
	m_is_armed = true;
	return true;
}
	
	bool
LOD_QSDecimator::
Step(
){
	return CollapseEdge();
}


LOD_QSDecimator::
LOD_QSDecimator(
	LOD_ManMesh2 &mesh,
	LOD_ExternNormalEditor &face_editor,
	LOD_ExternBufferEditor &extern_editor
) :
	m_is_armed (false),
	m_mesh(mesh),
	m_face_editor(face_editor),
	m_extern_editor(extern_editor)
{	
	m_deg_edges.reserve(32);
	m_deg_faces.reserve(32);
	m_deg_vertices.reserve(32);
	m_update_faces.reserve(32);
	m_new_edges.reserve(32);
	m_update_vertices.reserve(32);
};

	bool
LOD_QSDecimator::
CollapseEdge(
){
	
	// find an edge to collapse
	
	// FIXME force an edge collapse
	// or return false

	std::vector<LOD_Edge> & edges = m_mesh.EdgeSet();
	std::vector<LOD_Vertex> & verts = m_mesh.VertexSet();
	std::vector<LOD_Quadric> & quadrics = m_quadric_editor->Quadrics();
	int size = edges.size();

	if (size == 0) return false;

	const int heap_top = m_heap->Top();

	if (heap_top == -1 || edges[heap_top].HeapKey() <= -MT_INFINITY) {
		return false;
	}
	
	// compute the target position
	MT_Vector3 new_vertex = m_quadric_editor->TargetVertex(edges[heap_top]);
	LOD_Quadric & q0 = quadrics[edges[heap_top].m_verts[0]];
	LOD_Quadric & q1 = quadrics[edges[heap_top].m_verts[1]];

	LOD_Vertex &v0 = verts[edges[heap_top].m_verts[0]];
	LOD_Vertex &v1 = verts[edges[heap_top].m_verts[1]];

	LOD_Quadric sum = q0;
	sum += q1;


	if (m_collapser->CollapseEdge(
			heap_top,
			m_mesh,
			m_deg_edges,
			m_deg_faces,
			m_deg_vertices,
			m_new_edges,
			m_update_faces,
			m_update_vertices
	)) {

		// assign new vertex position

		 v0.pos = new_vertex;
		 v1.pos = new_vertex;

		// sum the quadrics of v0 and v1
		q0 = sum;
		q1 = sum;

		// ok update the primitive properties

		m_face_editor.Update(m_update_faces);	
		m_face_editor.UpdateVertexNormals(m_update_vertices);

		// update the external vertex buffer 
		m_extern_editor.CopyModifiedVerts(m_mesh,m_update_vertices);

		// update the external face buffer
		m_extern_editor.CopyModifiedFaces(m_mesh,m_update_faces);

		// update the edge heap
		UpdateHeap(m_deg_edges,m_new_edges);

		m_quadric_editor->Remove(m_deg_vertices);
		m_face_editor.Remove(m_deg_faces);
		m_face_editor.RemoveVertexNormals(m_deg_vertices);		
				
		// delete the primitives

		DeletePrimitives(m_deg_edges,m_deg_faces,m_deg_vertices);

	} else {
		// the edge could not be collapsed at the moment - so
		// we adjust it's priority and add it back to the heap.
		m_heap->Remove(&edges[0],0);
		edges[heap_top].HeapKey() = - MT_INFINITY;
		m_heap->Insert(&edges[0],heap_top);
	}

	//clear all the temporary buffers

	m_deg_faces.clear();
	m_deg_edges.clear();
	m_deg_vertices.clear();
	
	m_update_faces.clear();
	m_update_vertices.clear();
	m_new_edges.clear();

	return true;

}	

	void
LOD_QSDecimator::
DeletePrimitives(
	const vector<LOD_EdgeInd> & degenerate_edges,
	const vector<LOD_FaceInd> & degenerate_faces,
	const vector<LOD_VertexInd> & degenerate_vertices
) {

	// assumes that the 3 vectors are sorted in descending order.

	// Delete Degnerate primitives
	//////////////////////////////


	// delete the old edges - we have to be very careful here
	// mesh.delete() swaps edges to be deleted with the last edge in 
	// the edge buffer. However the next edge to be deleted may have
	// been the last edge in the buffer!

	// One way to solve this is to sort degenerate_edges in descending order.
	// And then delete them in that order.
	
	// it is also vital that degenerate_edges contains no duplicates

	vector<LOD_EdgeInd>::const_iterator edge_it = degenerate_edges.begin();
	vector<LOD_EdgeInd>::const_iterator edge_end = degenerate_edges.end();

	for (; edge_it != edge_end; ++edge_it) {
		m_mesh.DeleteEdge(*edge_it,m_heap);
	}



	vector<LOD_FaceInd>::const_iterator face_it = degenerate_faces.begin();
	vector<LOD_FaceInd>::const_iterator face_end = degenerate_faces.end();
	
	for (;face_it != face_end; ++face_it) {
		m_mesh.DeleteFace(m_extern_editor,*face_it);
	}

	vector<LOD_VertexInd>::const_iterator vertex_it = degenerate_vertices.begin();
	vector<LOD_VertexInd>::const_iterator vertex_end = degenerate_vertices.end();
	
	for (;vertex_it != vertex_end; ++vertex_it) {
		m_mesh.DeleteVertex(m_extern_editor,*vertex_it);
	}
}


	bool
LOD_QSDecimator::
BuildHeap(
){
	// build the quadrics 

	if (m_quadric_editor->BuildQuadrics(m_face_editor,true) == false) return false;


	m_heap = CTR_UHeap<LOD_Edge>::New();
	// load in edge pointers to the heap

	std::vector<LOD_Edge> & edge_set= m_mesh.EdgeSet();
	std::vector<LOD_Edge>::const_iterator edge_end = edge_set.end();
	std::vector<LOD_Edge>::iterator edge_start = edge_set.begin();

	std::vector<int> & heap_vector = m_heap->HeapVector();

	for (unsigned int i = 0; i < edge_set.size(); ++i) {
		edge_set[i].HeapPos() = i;
		heap_vector.push_back(i);
	}
	
	m_heap->MakeHeap(&edge_set[0]);

	return true;
}

	void
LOD_QSDecimator::
UpdateHeap(
	std::vector<LOD_EdgeInd> &deg_edges,
	std::vector<LOD_EdgeInd> &new_edges
){
	// first of all compute values for the new edges 
	// and bung them on the heap.

	std::vector<LOD_Edge>  & edge_set= m_mesh.EdgeSet();

	std::vector<LOD_EdgeInd>::const_iterator edge_it = new_edges.begin();
	std::vector<LOD_EdgeInd>::const_iterator end_it = new_edges.end();


	// insert all the new edges		
	///////////////////////////

	// compute edge costs ffor the new edges

	m_quadric_editor->ComputeEdgeCosts(new_edges);

	// inser the new elements into the heap

	for (; edge_it != end_it; ++edge_it) {		
		m_heap->Insert(&edge_set[0],*edge_it);
	}


	// remove all the old values from the heap

	edge_it = deg_edges.begin();
	end_it = deg_edges.end();

	for (; edge_it != end_it; ++edge_it) {
		LOD_Edge &e = edge_set[*edge_it];
		m_heap->Remove(&edge_set[0],e.HeapPos());

		e.HeapPos() = -1;

	}
}
	
