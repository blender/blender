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

#ifndef NAN_INCLUDED_BSP_CSGMesh_h
#define NAN_INCLUDED_BSP_CSGMesh_h

#include "BSP_MeshPrimitives.h"
#include "MEM_SmartPtr.h"
#include "MEM_RefCountPtr.h"
#include "MEM_NonCopyable.h"
#include "../extern/CSG_BooleanOps.h"


class MT_Plane3;

class BSP_CSGMesh : 
	public MEM_NonCopyable, 
	public MEM_RefCountable
{

public :

	static
		BSP_CSGMesh *
	New(
	);

		bool
	SetVertices(
		std::vector<BSP_MVertex> *verts
	);

		void
	AddPolygon(
		const int * verts,
		int num_verts
	);	

	// assumes that the face already has a plane equation
		void
	AddPolygon(
		const BSP_MFace &face
	);


	// Allocate and build the mesh edges.
	////////////////////////////////////

		bool
	BuildEdges(
	);

	// Clean the mesh of edges. and edge pointers
	// This removes the circular connectivity information
	/////////////////////////////////////////////

		void
	DestroyEdges(
	);

	// return a new seperate copy of the 
	// mesh allocated on the heap.

		BSP_CSGMesh *
	NewCopy(
	) const;
	

	// Reverse the winding order of every polygon 
	// in the mesh and swap the planes around.

		void
	Invert(
	);


	// geometry access
	//////////////////

		std::vector<BSP_MVertex> &
	VertexSet(
	) const ;		

		std::vector<BSP_MFace> &
	FaceSet(
	) const ;

		std::vector<BSP_MEdge> &
	EdgeSet(
	) const;

	~BSP_CSGMesh(
	);

	// local geometry queries.
	/////////////////////////

	// face queries
	///////////////

		void
	FaceVertices(
		const BSP_FaceInd & f,
		std::vector<BSP_VertexInd> &output
	);
	
		void
	FaceEdges(
		const BSP_FaceInd & f,
		std::vector<BSP_EdgeInd> &output
	);	

	// edge queries
	///////////////

		void
	EdgeVertices(
		const BSP_EdgeInd & e,
		std::vector<BSP_VertexInd> &output
	);

		void
	EdgeFaces(
		const BSP_EdgeInd & e,
		std::vector<BSP_FaceInd> &output
	);

	// vertex queries
	/////////////////

		void
	VertexEdges(
		const BSP_VertexInd & v,
		std::vector<BSP_EdgeInd> &output
	);
	
		void
	VertexFaces(
		const BSP_VertexInd & v,
		std::vector<BSP_FaceInd> &output
	);

	// Returns the edge index of the edge from v1 to v2. 
	// Does this by searching the edge sets of v1 - but not v2.
	// If you are paranoid you should check both and make sure the 
	// indices are the same. If the edge doe not exist edgeInd is empty.

		BSP_EdgeInd
	FindEdge(
		const BSP_VertexInd &v1,
		const BSP_VertexInd &v2
	) const;


	/**
	 * Sanity checkers
	 */

	// make sure the edge faces have a pointer to f

		bool
	SC_Face(
		BSP_FaceInd f
	);

	/**
	 * Return the face plane equation
	 */

		MT_Plane3
	FacePlane(
		const BSP_FaceInd &fi
	)const;


	/**
	 * Recompute Face plane equations.
	 * essential if you have been messing with the object.
	 */

		void
	ComputeFacePlanes(
	);
		
	/**
	 * Count the number of trinagles in the mesh.
	 * This is not the same as the number of polygons.
	 */

		int
	CountTriangles(
	) const;

private :
	
		void
	InsertEdge(
		const BSP_VertexInd &v1,
		const BSP_VertexInd &v2,
		const BSP_FaceInd &f,
		std::vector<BSP_EdgeInd> &new_edges
	);

		
	// Private to insure heap instantiation.

	BSP_CSGMesh(
	);

	std::vector<BSP_MVertex> *m_verts;
	std::vector<BSP_MFace>   *m_faces;
	std::vector<BSP_MEdge>   *m_edges;

	MT_Vector3 m_bbox_min;
	MT_Vector3 m_bbox_max;

};


#endif

