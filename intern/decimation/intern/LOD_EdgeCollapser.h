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

/** \file decimation/intern/LOD_EdgeCollapser.h
 *  \ingroup decimation
 */


#ifndef __LOD_EDGECOLLAPSER_H__
#define __LOD_EDGECOLLAPSER_H__

// This is a helper class that collapses edges of a 2 - manifold mesh.

#include "LOD_MeshPrimitives.h"
#include "MEM_NonCopyable.h"
#include <vector>
#include <functional>

class LOD_ManMesh2;

class LOD_EdgeCollapser 
: public  MEM_NonCopyable
{

public :
			
	static
		LOD_EdgeCollapser * 
	New(
	);

	// returns via arguments the set of modified
	// verts,edges and faces.

		bool
	CollapseEdge(
		LOD_EdgeInd ei,
		LOD_ManMesh2 &mesh,
		std::vector<LOD_EdgeInd> &	degenerate_edges,
		std::vector<LOD_FaceInd> &	degenerate_faces,
		std::vector<LOD_VertexInd> & degenerate_vertices,
		std::vector<LOD_EdgeInd> &	new_edges,
		std::vector<LOD_FaceInd> &	update_faces,
		std::vector<LOD_VertexInd> & update_vertices
	);

private :

	LOD_EdgeCollapser(
	);

	// Test to see if the result of collapsing the
	// edge produces 2 junctions in the mesh i.e. where
	// an edge is shared by more than 2 polygons

	// We count the number of coincedent edge pairs that
	// result from the collapse of collapse_edge.

	// If collapse edge is a boundary edge then the number of
	// coincedent pairs should be 1
	// else it should be 2.

		bool
	TJunctionTest(
		LOD_ManMesh2 &mesh,
		std::vector<LOD_EdgeInd> &e_v0v1,
		LOD_EdgeInd collapse_edge
	);

	// here's the definition of the sort function
	// we use to determine coincedent edges

	// assumes the edges are normalized i.e. m_verts[0] <= m_verts[1]

	struct less : std::binary_function<LOD_Edge, LOD_Edge, bool> {
			bool 
		operator()(
			const LOD_Edge& a,
			const LOD_Edge& b
		) const {
				
			if (int(a.m_verts[0]) == int(b.m_verts[0])) {
				return (int(a.m_verts[1]) < int(b.m_verts[1]));
			} else {
				return (int(a.m_verts[0]) < int(b.m_verts[0]));
			}
		}
	};

};

#endif

