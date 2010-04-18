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

#include "LOD_MeshPrimitives.h"

#include "MT_assert.h"
#include "LOD_MeshException.h"
#include <algorithm>

using namespace std;

// Vertex Methods
/////////////////

LOD_Vertex::
LOD_Vertex(
) :
	pos (MT_Vector3()),
	m_select_tag(false)
{
};

	bool
LOD_Vertex::
RemoveEdge(
	LOD_EdgeInd e
){

	vector<LOD_EdgeInd>::iterator result = find(m_edges.begin(),m_edges.end(),e);
	if (result == m_edges.end()) {
		return false;
	}

	std::swap(*result, m_edges.back());
	m_edges.pop_back();
	return true;	
};	

	void
LOD_Vertex::
AddEdge(
	LOD_EdgeInd e
){
	m_edges.push_back(e);
};

	void
LOD_Vertex::
SwapEdge(
	LOD_EdgeInd e_old,
	LOD_EdgeInd e_new
){

	vector<LOD_EdgeInd>::iterator result = 
		find(m_edges.begin(),m_edges.end(),e_old);
	if (result == m_edges.end()) {
		MT_assert(false);
		LOD_MeshException e(LOD_MeshException::e_search_error);
		throw(e);	
	}
	
	*result = e_new;
};

	bool
LOD_Vertex::
SelectTag(
) const {
	return m_select_tag;
};

	void
LOD_Vertex::
SetSelectTag(
	bool tag	
){
	m_select_tag = tag;
};

	bool
LOD_Vertex::
Degenerate(
){
	return m_edges.empty();
}

	void
LOD_Vertex::
CopyPosition(
	float *float_ptr
){
	pos.getValue(float_ptr);
}



// Edge Methods
///////////////

LOD_Edge::
LOD_Edge (
) {
	m_verts[0] = m_verts[1] = LOD_VertexInd::Empty();
	m_faces[0] = m_faces[1] = LOD_FaceInd::Empty();
}
		
	bool 
LOD_Edge::
operator == (
	LOD_Edge & rhs
) {
	// edges are the same if their vertex indices are the 
	// same!!! Other properties are not checked 

	int matches = 0;

	if (this->m_verts[0] == rhs.m_verts[0]) {
		++matches;
	}
	if (this->m_verts[1] == rhs.m_verts[0]) {
		++matches;
	}
	if (this->m_verts[0] == rhs.m_verts[1]) {
		++matches;
	}
	if (this->m_verts[1] == rhs.m_verts[1]) {
		++matches;
	}
	
	if (matches >= 2) {
		return true;
	}
	return false;
}

// Elementary helper methods
////////////////////////////

	LOD_FaceInd
LOD_Edge::
OpFace(
	LOD_FaceInd f
) const {
	if (f == m_faces[0]) {
		return m_faces[1];
	} else 
	if (f == m_faces[1]) {
		return m_faces[0];
	} else {
		MT_assert(false);
		LOD_MeshException e(LOD_MeshException::e_search_error);
		throw(e);	

		return LOD_FaceInd::Empty();
	}
}	

	void
LOD_Edge::
SwapFace(
	LOD_FaceInd old_f,
	LOD_FaceInd new_f
) {
	if (old_f == m_faces[0]) {
		m_faces[0] = new_f;
	} else 
	if (old_f == m_faces[1]) {
		m_faces[1] = new_f;
	} else {
		LOD_MeshException e(LOD_MeshException::e_search_error);
		throw(e);	
	}
}


// return the half edge face - the half edge is defined
// by the {vertex,edge} tuple. 

	LOD_FaceInd
LOD_Edge::
HalfEdgeFace(
	LOD_VertexInd vi
){
	if (vi == m_verts[0]) return m_faces[0];
	if (vi == m_verts[1]) return m_faces[1];
	MT_assert(false);
	
	LOD_MeshException e(LOD_MeshException::e_search_error);
	throw(e);	

	return LOD_FaceInd::Empty();
}	


	LOD_VertexInd
LOD_Edge::
OpVertex(
	LOD_VertexInd vi
) {
	if (vi == m_verts[0]) return m_verts[1];
	if (vi == m_verts[1]) return m_verts[0];
	MT_assert(false);

	LOD_MeshException e(LOD_MeshException::e_search_error);
	throw(e);	

	return LOD_VertexInd::Empty();
}

// replace the vertex v_old with vertex v_new
// error if v_old is not one of the original vertices

	void
LOD_Edge::
SwapVertex(
	LOD_VertexInd v_old,
	LOD_VertexInd v_new
) {
	if (v_old == m_verts[0]) {
		m_verts[0] = v_new;
	} else
	if (v_old == m_verts[1]) {
		m_verts[1] = v_new;
	} else {

		MT_assert(false);

		LOD_MeshException e(LOD_MeshException::e_search_error);
		throw(e);	
	}
	if(m_verts[0] == m_verts[1]) {
		MT_assert(false);

		LOD_MeshException e(LOD_MeshException::e_non_manifold);
		throw(e);	
	}

}			

	bool
LOD_Edge::
SelectTag(
) const {
	return bool(m_verts[1].Tag() & 0x1);
};

	void
LOD_Edge::
SetSelectTag(
	bool tag
) {
	m_verts[1].SetTag(int(tag));
};

	int
LOD_Edge::
OpenTag(
) const {
	return m_faces[0].Tag();
}

	void
LOD_Edge::
SetOpenTag(
	int tag
) {
	m_faces[0].SetTag(tag);
}

	bool
LOD_Edge::
Degenerate(
) const {
	return (
		(m_faces[0].IsEmpty() && m_faces[1].IsEmpty()) ||
		(m_verts[0] == m_verts[1])
	);
};

// TriFace Methods
//////////////////

LOD_TriFace::
LOD_TriFace(
) {
	m_verts[0] = m_verts[1] = m_verts[2] = LOD_VertexInd::Empty();
}

// Elementary helper methods
////////////////////////////

	void
LOD_TriFace::
SwapVertex(
	LOD_VertexInd old_v,
	LOD_VertexInd new_v
) {
	// could save branching here...

	if (m_verts[0] == old_v) {
		m_verts[0] = new_v;
	} else 
	if (m_verts[1] == old_v) {
		m_verts[1] = new_v;
	} else 
	if (m_verts[2] == old_v) {
		m_verts[2] = new_v;
	} else {
		MT_assert(false);

		LOD_MeshException excep(LOD_MeshException::e_search_error);
		throw(excep);	
	}
}

	bool
LOD_TriFace::
SelectTag(
) const {
	return bool(m_verts[1].Tag() & 0x1);
};

	void
LOD_TriFace::
SetSelectTag(
	bool tag
) {
	m_verts[1].SetTag(int(tag));
};

	int
LOD_TriFace::
OpenTag(
) {
	return m_verts[2].Tag();
}

	void
LOD_TriFace::
SetOpenTag(
	int tag
) {
	m_verts[2].SetTag(tag);
}

	bool
LOD_TriFace::
Degenerate(
) {

	return (
		(m_verts[0] == m_verts[1]) ||
		(m_verts[1] == m_verts[2]) ||
		(m_verts[2] == m_verts[0]) 
	);
}

	void
LOD_TriFace::
CopyVerts(
	int * index_ptr
){
	index_ptr[0] = m_verts[0];
	index_ptr[1] = m_verts[1];
	index_ptr[2] = m_verts[2];
};









