/**
 * $Id$
 *
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
#ifndef __RAS_POLYGON
#define __RAS_POLYGON

#include "RAS_TexVert.h"
#include "RAS_MaterialBucket.h"

#include <vector>
using namespace std;


//
// Bitfield that stores the flags for each CValue derived class
//
struct PolygonFlags {
	PolygonFlags() :
		Visible(true),
		Collider(true)
	{
	}
	unsigned char Visible : 1;
	unsigned char Collider : 1;
	//int Visible : 1;
	//int Collider : 1;
};

class RAS_Polygon
{
	RAS_MaterialBucket*			m_bucket;
	KX_VertexIndex				m_vertexindexbase;
	int							m_numverts;
	int							m_edgecode;
	PolygonFlags				m_polyFlags;
	

public:
	RAS_Polygon(RAS_MaterialBucket* bucket,
				bool visible,
				int numverts,
				int vtxarrayindex) ;
	virtual ~RAS_Polygon() {};
	
//	RAS_TexVert* GetVertex(int index);
	int					VertexCount();
	void				SetVertex(int i, unsigned int vertexindex); //const MT_Point3& xyz,const MT_Point2& uv,const unsigned int rgbacolor,const MT_Vector3& normal)
	
	const KX_VertexIndex& GetIndexBase();

	void				SetVisibleWireframeEdges(int edgecode);
	// each bit is for a visible edge, starting with bit 1 for the first edge, bit 2 for second etc.
	int					GetEdgeCode();
	
	bool				IsVisible();
	bool				IsCollider();
	void				SetCollider(bool col);

	KX_VertexIndex&		GetVertexIndexBase();
	RAS_MaterialBucket*	GetMaterial();

};

#endif

