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

#ifndef BSP_CSGNCMeshSplitter_h
#define BSP_CSGNCMeshSplitter_h

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
 * in a convex cell) by a plane. This class makes no attempt
 * to maintain edge connectivity in the mesh. It just rips
 * up the polygons. This is fine for tree building.
 */


class BSP_CSGNCMeshSplitter : public BSP_CSGISplitter
{
public :

	/// construction

	BSP_CSGNCMeshSplitter(
	);

	BSP_CSGNCMeshSplitter(
		const BSP_CSGNCMeshSplitter & other
	);

	/**
	 *  @section BSP specific mesh operations.
	 * Inherited from BSP_CSGISplitter
	 */
	
	/**
	 * Split a mesh fragment wrt plane. Generates 3 mesh fragments,
	 * in, out and on. Only splits polygons - not edges, does not maintain
	 * connectivity information. The contents of frag are consumed by this oepration.
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

	~BSP_CSGNCMeshSplitter(
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

		void
	SplitPolygon(
		const MT_Plane3 &plane,
		BSP_CSGMesh & mesh,
		BSP_FaceInd fi,
		BSP_FaceInd &fin,
		BSP_FaceInd &fout
	);

	/// Cached helpers

	/// Split function responsibe for:
	std::vector<BSP_FaceInd> m_spanning_faces;
	std::vector<BSP_VertexInd> m_tagged_verts;

	/// SplitPolygon responsible for:
	std::vector<BSP_FaceInd> m_in_loop,m_out_loop,m_on_loop;

};


#endif

