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

#ifdef WIN32
#pragma warning (disable:4786)
#endif

#include "RAS_Polygon.h"

RAS_Polygon::RAS_Polygon(RAS_MaterialBucket* bucket, RAS_DisplayArray *darray, int numvert)
{
	m_bucket = bucket;
	m_darray = darray;
	m_offset[0]= m_offset[1]= m_offset[2]= m_offset[3]= 0;
	m_numvert = numvert;

	m_edgecode = 255;
	m_polyflags = 0;
}

int RAS_Polygon::VertexCount()
{
	return m_numvert;
}

void RAS_Polygon::SetVertexOffset(int i, unsigned short offset)
{
	m_offset[i] = offset;
}

RAS_TexVert *RAS_Polygon::GetVertex(int i)
{
	return &m_darray->m_vertex[m_offset[i]];
}

int RAS_Polygon::GetVertexOffset(int i)
{
	return m_offset[i];
}

int RAS_Polygon::GetEdgeCode()
{
	return m_edgecode;
}

void RAS_Polygon::SetEdgeCode(int edgecode)
{
	m_edgecode = edgecode;
}

	
bool RAS_Polygon::IsVisible()
{
	return (m_polyflags & VISIBLE) != 0;
}

void RAS_Polygon::SetVisible(bool visible)
{
	if(visible) m_polyflags |= VISIBLE;
	else m_polyflags &= ~VISIBLE;
}

bool RAS_Polygon::IsCollider()
{
	return (m_polyflags & COLLIDER) != 0;
}

void RAS_Polygon::SetCollider(bool visible)
{
	if(visible) m_polyflags |= COLLIDER;
	else m_polyflags &= ~COLLIDER;
}

RAS_MaterialBucket* RAS_Polygon::GetMaterial()
{
	return m_bucket;
}

RAS_DisplayArray* RAS_Polygon::GetDisplayArray()
{
	return m_darray;
}
