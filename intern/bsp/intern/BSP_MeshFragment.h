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

#ifndef NAN_INCLUDED_BSP_MeshFragment_h

#define NAN_INCLUDED_BSP_MeshFragment_h

#define BSP_SPLIT_EPSILON MT_Scalar(1e-5)

#include <vector>
#include "BSP_MeshPrimitives.h"


class BSP_CSGMesh;
class MT_Plane3;
/**
 * This class encodes a collection of polygons from a mesh.
 * This class only remains valid when mesh indices do not change
 * internally and of course whilst the mesh is still around.
 *
 * Polygons in the mesh point back to the unique mesh fragment
 * containing them. 
 */


class BSP_MeshFragment  {
public:

	BSP_MeshFragment(
		BSP_CSGMesh *mesh,
		BSP_Classification classification
	);

		std::vector<BSP_FaceInd> &
	FaceSet(
	) ;

	const 
		std::vector<BSP_FaceInd> &
	FaceSet(
	) const ;

		BSP_CSGMesh *
	Mesh(
	);

		BSP_CSGMesh *
	Mesh(
	) const;

	
	// Classify this mesh fragment with respect
	// to plane. The index sets of this fragment
	// are consumed by this action. Vertices
	// are tagged with a classification enum.

		void
	Classify(
		const MT_Plane3 & plane,
		BSP_MeshFragment * in_frag,
 		BSP_MeshFragment * out_frag,
		BSP_MeshFragment * on_frag,
		std::vector<BSP_FaceInd> & spanning_faces,
		std::vector<BSP_VertexInd> & visited_verts
	);
	
	// Classify all the vertices and faces of mesh, generate
	// in,out and on mesh fragments.

	static
		void
	Classify(
		BSP_CSGMesh & mesh,
		const MT_Plane3 & plane,
		BSP_MeshFragment * in_frag,
 		BSP_MeshFragment * out_frag,
		BSP_MeshFragment * on_frag,
		std::vector<BSP_FaceInd> & spanning_faces,
		std::vector<BSP_VertexInd> & visited_verts
	);

	// Classify the on fragment into
	// 2 sets, the +ve on frags those whose polygon
	// normals point in the same direction as the plane,
	// and the -ve frag whose normals point in the other direction.

		void
	ClassifyOnFragments(
		const MT_Plane3 &plane,
		BSP_MeshFragment *pos_frag,
		BSP_MeshFragment *neg_frag
	);


	~BSP_MeshFragment(
	);

	/**
	 * Sanity checkers.
	 */


private:

	// Classify the polygon wrt to the plane
	static
		BSP_Classification 
	ClassifyPolygon(
		const MT_Plane3 &plane,
		const BSP_MFace & face,
		std::vector<BSP_MVertex>::const_iterator verts,
		std::vector<BSP_VertexInd> & visited_verts
	);

private :


	BSP_CSGMesh * m_mesh;
	BSP_Classification m_classification;
	std::vector<BSP_FaceInd> m_faces;
};


#endif
