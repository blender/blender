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

#ifndef NAN_INCLUDED_BSP_CSGMesh_h

#define NAN_INCLUDED_BSP_CSGMesh_h

#include "BSP_MeshPrimitives.h"
#include "MEM_SmartPtr.h"
#include "MEM_RefCountPtr.h"
#include "MEM_NonCopyable.h"
#include "BSP_CSGUserData.h"
#include "../extern/CSG_BooleanOps.h"


class MT_Plane3;
class BSP_MeshFragment;

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
		MEM_SmartPtr<std::vector<BSP_MVertex> > verts
	);

		void
	SetFaceVertexData(
		MEM_SmartPtr<BSP_CSGUserData> fv_data
	);

		void
	SetFaceData(
		MEM_SmartPtr<BSP_CSGUserData> f_data
	);

		void
	AddPolygon(
		const int * verts,
		int num_verts
	);	

		void
	AddPolygon(
		const int * verts,
		const int * fv_indices,
		int num_verts
	);

		void
	AddSubTriangle(
		const BSP_MFace &iface,
		const int * index_info
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

		BSP_CSGUserData &
	FaceVertexData(
	) const;

		BSP_CSGUserData &
	FaceData(
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


	// Bounding box methods
	///////////////////////

		void
	SetBBox(
		const MT_Vector3 & min,
		const MT_Vector3 & max
	);

		void
	BBox(
		MT_Vector3 &min,
		MT_Vector3 &max
	) const ;

	// Update the BBox
	//////////////////

		void
	UpdateBBox(
	);

	
	/**
	 * Sanity checkers
	 */

	// make sure the edge faces have a pointer to f

		bool
	SC_Face(
		BSP_FaceInd f
	);

	/**
	 * Make sure the polygons vertex classification is correct
	 */

		void
	SC_Classification(
		BSP_FaceInd f,
		const MT_Plane3&plane
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

	/** 
	 * Insert a vertex index into a polygon
	 * and call the external splitting function to 
	 * generate a new face vertex property.
	 */

		void
	InsertVertexIntoFace(
		BSP_MFace & face,
		const BSP_VertexInd & v1,
		const BSP_VertexInd & v2,
		const BSP_VertexInd & vi,
		CSG_InterpolateUserFaceVertexDataFunc fv_split_func,
		MT_Scalar epsilon
	);


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


	MEM_SmartPtr< std::vector<BSP_MVertex> > m_verts;
	MEM_SmartPtr< std::vector<BSP_MFace> > m_faces;
	MEM_SmartPtr< std::vector<BSP_MEdge> > m_edges;

	// The face_vertex user data associated with this mesh

	MEM_SmartPtr<BSP_CSGUserData> m_fv_data;

	// The face user data associated with this mesh - 
	// This is a buffer that maps directly to the face buffer.
	// An index into the faces is alos an index into m_face_data 
	// for that face

	MEM_SmartPtr<BSP_CSGUserData> m_face_data;

	
	MT_Vector3 m_bbox_min;
	MT_Vector3 m_bbox_max;

};


#endif