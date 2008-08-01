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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32

#pragma warning (disable:4786)
#endif

#include "RAS_Polygon.h"

RAS_Polygon::RAS_Polygon(RAS_MaterialBucket* bucket,
				bool visible,
				int numverts,
				int vtxarrayindex) 
		:m_bucket(bucket),
		m_vertexindexbase(numverts),
		m_numverts(numverts),
		m_edgecode(65535)
{
	m_vertexindexbase.m_vtxarray = vtxarrayindex ;//m_bucket->FindVertexArray(numverts);
	m_polyFlags.Visible = visible;
}



int RAS_Polygon::VertexCount()
{
	return m_numverts;
}



void RAS_Polygon::SetVertex(int i,
						unsigned int vertexindex ) //const MT_Point3& xyz,const MT_Point2& uv,const unsigned int rgbacolor,const MT_Vector3& normal)
{
	m_vertexindexbase.SetIndex(i,vertexindex); //m_bucket->FindOrAddVertex(m_vertexindexbase.m_vtxarray,
	//xyz,uv,rgbacolor,normal));
}



const KX_VertexIndex& RAS_Polygon::GetIndexBase()
{
	return m_vertexindexbase;
}



void RAS_Polygon::SetVisibleWireframeEdges(int edgecode)
{
	m_edgecode = edgecode;
}



// each bit is for a visible edge, starting with bit 1 for the first edge, bit 2 for second etc.
int RAS_Polygon::GetEdgeCode()
{
	return m_edgecode;
}


	
bool RAS_Polygon::IsVisible()
{
	return m_polyFlags.Visible;
}



bool RAS_Polygon::IsCollider()
{
	return m_polyFlags.Collider;
}



void RAS_Polygon::SetCollider(bool col)
{
	m_polyFlags.Collider = col;
}



KX_VertexIndex& RAS_Polygon::GetVertexIndexBase()
{
	return m_vertexindexbase;
}



RAS_MaterialBucket*	RAS_Polygon::GetMaterial()
{
	return m_bucket;
}
