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

#ifndef BSP_CSGMeshSplitter_h

#define BSP_CSGMeshSplitter_h

class BSP_MeshFragment;
class MT_Plane3;
class BSP_CSGMesh;

#include "BSP_MeshPrimitives.h"
#include "../extern/CSG_BooleanOps.h"
#include "BSP_CSGISplitter.h"


/**
 * This class contains splitting functions for a CSGMesh.
 * The atomic operation of a bsp CSG algorithm is to split 
 * a mesh fragment (connected collection of polygons contained
 * in a convex cell) by a plane. It is vital to leave the 
 * CSGMesh in a valid state after each such operation 
 * this class insures this (or tries it's best!).
 */


class BSP_CSGMeshSplitter : public BSP_CSGISplitter
{
public :

	/// construction

	BSP_CSGMeshSplitter(
		CSG_InterpolateUserFaceVertexDataFunc fv_split_func
	);

	BSP_CSGMeshSplitter(
		const BSP_CSGMeshSplitter & other
	);

	/**
	 *  @section BSP specific mesh operations.
	 * Inherited from BSP_CSGISplitter
	 */
	
	/**
	 * Split a mesh fragment wrt plane. Generates 3 mesh fragments,
	 * in, out and on. Makes sure the mesh is coherent after the
	 * operation. The contents of frag are consumed by this oepration.
	 */

		void
	Split(
		const MT_Plane3& plane,
		BSP_MeshFragment *frag,
		BSP_MeshFragment *in_frag,
		BSP_MeshFragment *out_frag,
		BSP_MeshFragment *on_frag,
		BSP_MeshFragment *spanning_frag
	);

	/// Split the entire mesh with respect to the plane.

		void
	Split(
		BSP_CSGMesh & mesh,
		const MT_Plane3& plane,
		BSP_MeshFragment *in_frag,
		BSP_MeshFragment *out_frag,
		BSP_MeshFragment *on_frag,
		BSP_MeshFragment *spanning_frag
	);

	~BSP_CSGMeshSplitter(
	);

private :

		void
	SplitImp(
		BSP_CSGMesh & mesh,
		const MT_Plane3& plane,
		const std::vector<BSP_FaceInd> & spanning_faces,
		BSP_MeshFragment *in_frag,
		BSP_MeshFragment *out_frag,
		BSP_MeshFragment *on_frag,
		std::vector<BSP_VertexInd> & classified_verts
	);		


	/**
	 * @section Atomic mesh operations.
	 */

	/**
	 * Add a vertex to the mesh, along
	 * a given edge. Leaves the mesh in a valid state.
	 * The vertex gets copied onto the back of the
	 * current vertex array. It's upto you to insure
	 * that the vertex actually lies on the edge and leaves
	 * the neighbouring faces convex. Returns the vertex index
	 * of the new vertex.
	 *
	 * epsilon is the relative distance [0,1] of the new
	 * vertex from the first vertex of the edge. This is
	 * used to intepolate face properties.
	 */

		BSP_VertexInd 
	SplitEdge(	
		BSP_CSGMesh & mesh,
		BSP_EdgeInd ei,
		BSP_MVertex &vertex,
		MT_Scalar epsilon
	);

	/**
	 * Split a polygon along an edge connecting the
	 * two tagged (on) vertices of the polygon. It assumes
	 * that you have already introduced two new vertex indices
	 * into the polygon that point to vertices tagged with 
	 * {in,out,on} information. It creates a new edge from the
	 * 2 on vertices that must be present. It then splits up
	 * the polygon into 2 fragments on the inside and outside.
	 * It returns 2 indices into the face list. 1 for the inside
	 * polygon and 1 for the outside polygon.
	 */

		void
	SplitPolygon(
		BSP_CSGMesh & mesh,
		BSP_FaceInd fi,
		BSP_FaceInd &fin,
		BSP_FaceInd &fout
	);

	/**
	 * Triangulate a convex quad (the maximum size polygon
	 * resulting from splitting a triangle). This can create up
	 * to one new face - which is added to the mesh. Note 
	 * that this method does not preserve references. It uses
	 * the edge which divides the quad into roughly equal triangular
	 * areas as the splitting edge. - This should avoid creating
	 * degenerate triangles.
	 */

		BSP_FaceInd
	TriangulateConvexQuad(
		BSP_CSGMesh & mesh,
		const BSP_FaceInd fi
	);

private :

	// The function pointer used to split face vertex properties.

	CSG_InterpolateUserFaceVertexDataFunc m_fv_func;

	/// Cached helpers

	/// Split function responsibe for:
	std::vector<BSP_FaceInd> m_spanning_faces;
	std::vector<BSP_VertexInd> m_tagged_verts;

	/// SplitImp responsible for:
	std::vector<BSP_EdgeInd> m_spanning_edges;
	// The list of faces affected by splitting the spanning edge set.
	std::vector<BSP_EdgeInd> m_visited_edges;
	std::vector<BSP_VertexInd> m_visited_verts;

	/// SplitPolygon responsible for:
	std::vector<BSP_FaceInd> m_in_loop,m_out_loop,m_on_loop;
	std::vector<BSP_UserFVInd> m_fv_in_loop,m_fv_out_loop;

};

#endif



