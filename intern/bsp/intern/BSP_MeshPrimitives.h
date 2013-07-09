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

/** \file bsp/intern/BSP_MeshPrimitives.h
 *  \ingroup bsp
 */


#ifndef __BSP_MESHPRIMITIVES_H__
#define __BSP_MESHPRIMITIVES_H__

#include "CTR_TaggedIndex.h"
#include "MT_Vector3.h"
#include "MT_Plane3.h"

#include <vector>

typedef CTR_TaggedIndex<24,0x00ffffff> BSP_VertexInd;
typedef CTR_TaggedIndex<24,0x00ffffff> BSP_EdgeInd;
typedef CTR_TaggedIndex<24,0x00ffffff> BSP_FaceInd;
typedef CTR_TaggedIndex<24,0x00ffffff> BSP_FragInd;


typedef std::vector<BSP_VertexInd> BSP_VertexList;
typedef std::vector<BSP_EdgeInd> BSP_EdgeList;
typedef std::vector<BSP_FaceInd> BSP_FaceList;

/** 
 * Enum representing classification of primitives 
 * with respect to a hyperplane.
 */

enum BSP_Classification{
	e_unclassified = 0,
	e_classified_in = 1,
	e_classified_out = 2,
	e_classified_on = 4,
	e_classified_spanning = 7
};

/**
 * @section Mesh linkage
 * The mesh is linked in a similar way to the decimation mesh,
 * although the primitives are a little more general and not
 * limited to manifold meshes.
 * Vertices -> (2+)Edges
 * Edges -> (1+)Polygons
 * Edges -> (2)Vertices.
 * Polygons -> (3+)Vertices.
 *
 * this structure allows for arbitrary polygons (assumed to be convex).
 * Edges can point to more than 2 polygons (non-manifold)
 *
 * We also define 2 different link types between edges and their
 * neighbouring polygons. A weak link and a strong link.
 * A weak link means the polygon is in a different mesh fragment
 * to the other polygon. A strong link means the polygon is in the
 * same fragment.
 * This is not entirely consistent as it means edges have to be associated
 * with fragments, in reality only polygons will be - edges and vertices
 * will live in global pools. I guess we should mark edges as being on plane
 * boundaries. This leaves a problem with non-manifold edges because for example
 * 3 of 4 possible edges could lie in 1 fragment and the remaining edge lie in
 * another, there is no way to work out then from one polygon which neighbouring
 * polygons are in the same/different mesh fragment.
 *
 * By definition an edge will only ever lie on 1 hyperplane. We can then just
 * tag neighbouring polygons with one of 3 tags to group them.
 */

class BSP_MVertex {
public :
	MT_Point3 m_pos;
	BSP_EdgeList m_edges;
	
	/**
	 * TODO 
	 * Is this boolean necessary or can we nick a few bits of m_edges[0]
	 * for example?
	 * The only problem with this is that if the vertex is degenerate then
	 * m_edges[0] may not exist. If the algorithm guarentees that this is 
	 * not the case then it should be changed.
	 */

	bool m_select_tag;
	int m_open_tag;

	BSP_MVertex(
	);

	BSP_MVertex(
		const MT_Point3 & pos
	);

		BSP_MVertex &
	operator = (
		const BSP_MVertex & other
	) {
		m_pos = other.m_pos;
		m_edges = other.m_edges;
		m_select_tag = other.m_select_tag;
		m_open_tag = other.m_open_tag;
		return (*this);
	};

		bool
	RemoveEdge(
		BSP_EdgeInd e
	);	

		void
	AddEdge(
		BSP_EdgeInd e
	);

		void
	SwapEdge(
		BSP_EdgeInd e_old,
		BSP_EdgeInd e_new
	);

	/** 
	 * These operations are ONLY valid when the
	 * vertex has some edges associated with it.
	 * This is left to the user to guarentee.
	 * Also note that these tag's are not guarenteed
	 * to survive after a call to RemoveEdge(),
	 * because we use edges for the open tag.
	 */

		int
	OpenTag(
	) const;

		void
	SetOpenTag(
		int tag
	);
		
		bool
	SelectTag(
	) const;

		void
	SetSelectTag(
		bool tag	
	);
};

class BSP_MEdge {
public :
	BSP_VertexInd m_verts[2];
	BSP_FaceList m_faces;

	BSP_MEdge(
	);

	bool operator == (
		BSP_MEdge & rhs
	);

		void
	SwapFace(
		BSP_FaceInd old_f,
		BSP_FaceInd new_f
	);

		BSP_VertexInd
	OpVertex(
		BSP_VertexInd vi
	) const;

		bool
	SelectTag(
	) const;

		void
	SetSelectTag(
		bool tag	
	);
	
	/**
	 * We use one of the vertex indices for tag informtaion.
	 * This means these tags will not survive if you change 
	 * the vertex indices.
	 */

		int
	OpenTag(
	) const;

		void
	SetOpenTag(
		int tag
	) ;
};

class BSP_MFace {
public :

	BSP_VertexList m_verts;

	// We also store the plane equation of this
	// face. Generating on the fly during tree
	// construction can lead to a lot of numerical errors.
	// because the polygon size can get very small.

	MT_Plane3 m_plane;

	int m_open_tag;
	unsigned int m_orig_face;

	BSP_MFace(
	);

	// Invert the face , done by reversing the vertex order 
	// and inverting the face normal.

		void
	Invert(
	);

	/**
	 * Tagging
	 * We use the tag from m_verts[1] for the select tag
	 * and the the tag from m_verts[0] for the open tag.
	 * There is always a chance that the polygon contains
	 * no vertices but this should be checked at construction
	 * time.
	 * Also note that changing the vertex indices of this polygon
	 * will likely remove tagging information.
	 *
	 */

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
	) ;

};

#endif

