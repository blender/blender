/*
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

/** \file decimation/intern/LOD_MeshPrimitives.h
 *  \ingroup decimation
 */


#ifndef __LOD_MESHPRIMITIVES_H__
#define __LOD_MESHPRIMITIVES_H__

#include "MT_Vector3.h"
#include "CTR_TaggedIndex.h"
#include "CTR_UHeap.h"
#include <vector>

typedef CTR_TaggedIndex<24,0x00ffffff> LOD_VertexInd;
typedef CTR_TaggedIndex<24,0x00ffffff> LOD_EdgeInd;
typedef CTR_TaggedIndex<24,0x00ffffff> LOD_FaceInd;
typedef CTR_TaggedIndex<24,0x00ffffff> LOD_HeapInd;

class LOD_Vertex {
public :
	MT_Vector3 pos;
	std::vector<LOD_EdgeInd> m_edges;
	bool m_select_tag;

	LOD_Vertex(
	);

		bool
	RemoveEdge(
		LOD_EdgeInd e
	);	

		void
	AddEdge(
		LOD_EdgeInd e
	);

		void
	SwapEdge(
		LOD_EdgeInd e_old,
		LOD_EdgeInd e_new
	);

		bool
	SelectTag(
	) const; 

		void
	SetSelectTag(
		bool tag	
	);

		bool
	Degenerate(
	);

		void
	CopyPosition(
		float *float_ptr
	);

private :


};

class LOD_Edge : public CTR_UHeapable  {
public :
	LOD_VertexInd	m_verts[2];
	LOD_FaceInd		m_faces[2];

	LOD_Edge (
	);
			
	bool operator == (
		LOD_Edge & rhs
	);

	// Elementary helper methods
	////////////////////////////

		LOD_FaceInd
	OpFace(
		LOD_FaceInd f
	) const;

		void
	SwapFace(
		LOD_FaceInd old_f,
		LOD_FaceInd new_f
	);


	// return the half edge face - the half edge is defined
	// by the {vertex,edge} tuple. 

		LOD_FaceInd
	HalfEdgeFace(
		LOD_VertexInd vi
	);	
	

		LOD_VertexInd
	OpVertex(
		LOD_VertexInd vi
	);

	// replace the vertex v_old with vertex v_new
	// error if v_old is not one of the original vertices

		void
	SwapVertex(
		LOD_VertexInd v_old,
		LOD_VertexInd v_new
	);

		bool
	SelectTag(
	) const;

		void
	SetSelectTag(
		bool tag
	);
	
		int
	OpenTag(
	) const;

		void
	SetOpenTag(
		int tag
	);

		bool
	Degenerate(
	) const;

		bool
	BoundaryEdge(
	) const {
		return (m_faces[0].IsEmpty() || m_faces[1].IsEmpty());
	};
	

};

class LOD_TriFace {
public:

	LOD_VertexInd m_verts[3];

	LOD_TriFace(
	);

	// Elementary helper methods
	////////////////////////////

		void
	SwapVertex(
		LOD_VertexInd old_v,
		LOD_VertexInd new_v
	);

		bool
	SelectTag(
	) const;

		void
	SetSelectTag(
		bool tag
	);

		int
	OpenTag(
	);
		void
	SetOpenTag(
		int tag
	);

		bool
	Degenerate(
	);

		void
	CopyVerts(
		int * index_ptr
	);

};
	
#endif

