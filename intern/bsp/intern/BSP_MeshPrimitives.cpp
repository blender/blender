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

#include "BSP_MeshPrimitives.h"

#include "MT_assert.h"
#include "BSP_CSGException.h"
#include <algorithm>

using namespace std;

BSP_MVertex::
BSP_MVertex(
) :
	m_pos (MT_Point3()),
	m_select_tag (false),
	m_open_tag (0)
{
};

BSP_MVertex::
BSP_MVertex(
	const MT_Point3 & pos
) :
	m_pos(pos),
	m_select_tag (false),
	m_open_tag (0)
{
};


	bool
BSP_MVertex::
RemoveEdge(
	BSP_EdgeInd e
){
	vector<BSP_EdgeInd>::iterator result = find(m_edges.begin(),m_edges.end(),e);
	if (result == m_edges.end()) {
		return false;
	}
	BSP_EdgeInd last = m_edges.back();
	m_edges.pop_back();
	if (m_edges.empty()) return true;

	*result = last;
	return true;	
}	

	void
BSP_MVertex::
AddEdge(
	BSP_EdgeInd e
){
	m_edges.push_back(e);
}

	void
BSP_MVertex::
SwapEdge(
	BSP_EdgeInd e_old,
	BSP_EdgeInd e_new
){
	vector<BSP_EdgeInd>::iterator result = 
		find(m_edges.begin(),m_edges.end(),e_old);
	if (result == m_edges.end()) {
		BSP_CSGException e(e_mesh_error);
		throw(e);
		MT_assert(false);
	}
	
	*result = e_new;
}

	bool
BSP_MVertex::
SelectTag(
) const{
	return m_select_tag;
}

	void
BSP_MVertex::
SetSelectTag(
	bool tag	
){
	m_select_tag = tag;
}

	int
BSP_MVertex::
OpenTag(
) const {
	return m_open_tag;
}

	void
BSP_MVertex::
SetOpenTag(
	int tag
){
	m_open_tag = tag;
}


/**
 * Edge Primitive Methods.
 */

BSP_MEdge::
BSP_MEdge(
){
	m_verts[0] = m_verts[1] = BSP_VertexInd::Empty();
}
	
	bool 
BSP_MEdge::
operator == (
	BSP_MEdge & rhs
){
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

	void
BSP_MEdge::
SwapFace(
	BSP_FaceInd old_f,
	BSP_FaceInd new_f
){
	vector<BSP_FaceInd>::iterator result = 
		find(m_faces.begin(),m_faces.end(),old_f);
	if (result == m_faces.end()) {
		BSP_CSGException e(e_mesh_error);
		throw(e);
		MT_assert(false);
	}
	
	*result = new_f;
}

    BSP_VertexInd
BSP_MEdge::
OpVertex(
	BSP_VertexInd vi
) const {
	if (vi == m_verts[0]) return m_verts[1];
	if (vi == m_verts[1]) return m_verts[0];
	MT_assert(false);
	BSP_CSGException e(e_mesh_error);
	throw(e);

	return BSP_VertexInd::Empty();
}

	bool
BSP_MEdge::
SelectTag(
) const {
	return bool(m_verts[1].Tag() & 0x1);
}
	void
BSP_MEdge::
SetSelectTag(
	bool tag	
){
	m_verts[1].SetTag(int(tag));
}

	int
BSP_MEdge::
OpenTag(
) const {
	return m_verts[0].Tag();
}

	void
BSP_MEdge::
SetOpenTag(
	int tag
) {
	// Note conversion from int to unsigned int!!!!!
	m_verts[0].SetTag(tag);
}
	

/**
 * Face primitive methods
 */


BSP_MFace::
BSP_MFace(
):
	m_open_tag(-1),
	m_orig_face(0)
{
	// nothing to do
}

	void
BSP_MFace::
Invert(
){

	// TODO replace reverse as I think some compilers
	// do not support the STL routines employed.

	reverse(
		m_verts.begin(),
		m_verts.end()
	);

	// invert the normal
	m_plane.Invert();
}

	bool
BSP_MFace::
SelectTag(
) const {
	return bool(m_verts[1].Tag() & 0x1);
}	

	void
BSP_MFace::
SetSelectTag(
	bool tag	
){
	m_verts[1].SetTag(int(tag));
};	

	int
BSP_MFace::
OpenTag(
) const {
	return m_open_tag;
}

	void
BSP_MFace::
SetOpenTag(
	int tag
){
	// Note conversion from int to unsigned int!!!!!
	m_open_tag = tag;
}



