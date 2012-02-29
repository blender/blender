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

/** \file decimation/intern/LOD_ManMesh2.h
 *  \ingroup decimation
 */


#ifndef __LOD_MANMESH2_H__
#define __LOD_MANMESH2_H__

#include "LOD_MeshPrimitives.h"
#include "MEM_SmartPtr.h"
#include <vector>

template <class HeapType> class CTR_UHeap;

class LOD_ExternBufferEditor;

class LOD_ManMesh2 // Manifold 2 dimensional mesh
{

public:

	static
		LOD_ManMesh2 *
	New(
	);

	// take ownership of the vertices.

		bool	
	SetVertices(
		MEM_SmartPtr<std::vector<LOD_Vertex> > verts
	);	
		
	// Add a triangle to the mesh

		void
	AddTriangle(
		int verts[3]
	);	

		void
	ConnectTriangle(
		LOD_FaceInd fi,
		std::vector<LOD_EdgeInd> & new_edges
	);
	
	// geometry access
	//////////////////

		std::vector<LOD_Vertex> &
	VertexSet(
	) const;

		std::vector<LOD_TriFace> &
	FaceSet(
	) const;

		std::vector<LOD_Edge> &
	EdgeSet(
	) const;

	~LOD_ManMesh2(
	);

	// local geometry queries
	/////////////////////////

	// face queries
	///////////////

		void
	FaceVertices(
		LOD_FaceInd f,
		std::vector<LOD_VertexInd> &output
	);
	
		void
	FaceEdges(
		LOD_FaceInd f,
		std::vector<LOD_EdgeInd> &output
	);	

	// edge queries
	///////////////

		void
	EdgeVertices(
		LOD_EdgeInd e,
		std::vector<LOD_VertexInd> &output
	);

		void
	EdgeFaces(
		LOD_EdgeInd e,
		std::vector<LOD_FaceInd> &output
	);

	// vertex queries
	/////////////////

		void
	VertexEdges(
		LOD_VertexInd v,
		std::vector<LOD_EdgeInd> &output
	);
	
		void
	VertexFaces(
		LOD_VertexInd v,
		std::vector<LOD_FaceInd> &output
	);

		void
	SetBBox(
		MT_Vector3 bbox_min,
		MT_Vector3 bbox_max
	);

		MT_Vector3
	BBoxMin(
	) const {
		return m_bbox_min;
	};
 
		MT_Vector3
	BBoxMax(
	) const {
		return m_bbox_max;
	};

	// Remove a primitive from the mesh
	///////////////////////////////////

	// These methods assume you have correctly
	// tidied up the index pointers in other primitives,
	// so that nothing refers to this object any more

	// These methods exchange the primitive with the 
	// last primitive in the vector. It modifies everything 
	// pointing to the last primitive correctly.

	// FIXME refactor extern editor out of primitive deletion
	// insead return a vector of primitives that need to be
	// modified and do this externally

		void
	DeleteVertex(
		LOD_ExternBufferEditor & extern_editor,
		LOD_VertexInd v
	);

		void
	DeleteEdge(
		LOD_EdgeInd e,
		CTR_UHeap<LOD_Edge> *heap
	);

		void
	DeleteFace(
		LOD_ExternBufferEditor & extern_editor,
		LOD_FaceInd f
	);

	// Sanity Check routines
	////////////////////////

	// Make sure the edge sets and the vertex sets are
	// consistent

		void
	SC_TriFace(
		LOD_FaceInd f
	);

	// basic sanity checking of an edge list bails out if there are more than 1024
	// edges
	
		void
	SC_EdgeList(
		LOD_EdgeInd e
	);


	// Check to see that the edges of v1 and v2 are unique.

		bool
	SC_UniqueEdge(
		LOD_EdgeInd e
	);


private :


	// Returns the edge index of the edge from v1 to v2. 
	// Does this by searching the edge sets of v1 - but not v2.
	// If you are paranoid you should check both and make sure the 
	// indices are the same. If the edge doe not exist edgeInd is empty.

		LOD_EdgeInd
	FindEdge(
		const LOD_VertexInd v1,
		const LOD_VertexInd v2
	);

	// Insert an edge into the mesh
	// Tie up the ptrs and create space for the edge
	// returns manifold errors - need to sort out memory edges

		bool
	InsertEdge(
		const LOD_VertexInd v1,
		const LOD_VertexInd v2,
		const LOD_FaceInd f,
		std::vector<LOD_EdgeInd> &new_edges
	);


private :

	LOD_ManMesh2(
	);

	MEM_SmartPtr< std::vector<LOD_Vertex> > m_verts;
	MEM_SmartPtr< std::vector<LOD_TriFace> > m_faces;
	MEM_SmartPtr< std::vector<LOD_Edge> > m_edges;

	// not sure of these descrtiptions of the mesh should
	// reside in this class coz may lead to very bloated interface.

	MT_Vector3 m_bbox_min;
	MT_Vector3 m_bbox_max;


};

#endif

