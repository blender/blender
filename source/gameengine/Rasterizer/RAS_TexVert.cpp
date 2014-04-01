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

/** \file gameengine/Rasterizer/RAS_TexVert.cpp
 *  \ingroup bgerast
 */


#include "RAS_TexVert.h"
#include "MT_Matrix4x4.h"
#include "BLI_math.h"

RAS_TexVert::RAS_TexVert(const MT_Point3& xyz,
                         const MT_Point2 uvs[MAX_UNIT],
                         const MT_Vector4& tangent,
                         const unsigned int rgba,
                         const MT_Vector3& normal,
                         const bool flat,
                         const unsigned int origindex)
{
	xyz.getValue(m_localxyz);
	SetRGBA(rgba);
	SetNormal(normal);
	tangent.getValue(m_tangent);
	m_flag = (flat) ? FLAT: 0;
	m_origindex = origindex;
	m_unit = 2;
	m_softBodyIndex = -1;

	for (int i = 0; i < MAX_UNIT; ++i)
	{
		uvs[i].getValue(m_uvs[i]);
	}
}

const MT_Point3& RAS_TexVert::xyz()
{
	g_pt3.setValue(m_localxyz);
	return g_pt3;
}

void RAS_TexVert::SetRGBA(const MT_Vector4& rgba)
{
	unsigned char *colp = (unsigned char*) &m_rgba;
	colp[0] = (unsigned char) (rgba[0] * 255.0);
	colp[1] = (unsigned char) (rgba[1] * 255.0);
	colp[2] = (unsigned char) (rgba[2] * 255.0);
	colp[3] = (unsigned char) (rgba[3] * 255.0);
}


void RAS_TexVert::SetXYZ(const MT_Point3& xyz)
{
	xyz.getValue(m_localxyz);
}

void RAS_TexVert::SetXYZ(const float xyz[3])
{
	copy_v3_v3(m_localxyz, xyz);
}

void RAS_TexVert::SetUV(int index, const MT_Point2& uv)
{
	uv.getValue(m_uvs[index]);
}

void RAS_TexVert::SetUV(int index, const float uv[2])
{
	copy_v2_v2(m_uvs[index], uv);
}

void RAS_TexVert::SetRGBA(const unsigned int rgba)
{ 
	m_rgba = rgba;
}


void RAS_TexVert::SetFlag(const short flag)
{
	m_flag = flag;
}

void RAS_TexVert::SetUnit(const unsigned int u)
{
	m_unit = (u <= (unsigned int) MAX_UNIT) ? u: (unsigned int)MAX_UNIT;
}

void RAS_TexVert::SetNormal(const MT_Vector3& normal)
{
	normal.getValue(m_normal);
}

void RAS_TexVert::SetTangent(const MT_Vector3& tangent)
{
	tangent.getValue(m_tangent);
}


// compare two vertices, and return true if both are almost identical (they can be shared)
bool RAS_TexVert::closeTo(const RAS_TexVert* other)
{
	const float eps = FLT_EPSILON;
	for (int i = 0; i < MAX_UNIT; i++) {
		if (!compare_v2v2(m_uvs[i], other->m_uvs[i], eps)) {
			return false;
		}
	}

	return (/* m_flag == other->m_flag && */
	        /* at the moment the face only stores the smooth/flat setting so don't bother comparing it */
	        (m_rgba == other->m_rgba) &&
	        compare_v3v3(m_normal, other->m_normal, eps) &&
	        compare_v3v3(m_tangent, other->m_tangent, eps)
	        /* don't bother comparing m_localxyz since we know there from the same vert */
	        /* && compare_v3v3(m_localxyz, other->m_localxyz, eps))*/
	        );
}

short RAS_TexVert::getFlag() const
{
	return m_flag;
}


unsigned int RAS_TexVert::getUnit() const
{
	return m_unit;
}

void RAS_TexVert::Transform(const MT_Matrix4x4& mat, const MT_Matrix4x4& nmat)
{
	SetXYZ((mat * MT_Vector4(m_localxyz[0], m_localxyz[1], m_localxyz[2], 1.0)).getValue());
	SetNormal((nmat * MT_Vector4(m_normal[0], m_normal[1], m_normal[2], 1.0)).getValue());
	SetTangent((nmat * MT_Vector4(m_tangent[0], m_tangent[1], m_tangent[2], 1.0)).getValue());
}

void RAS_TexVert::TransformUV(int index, const MT_Matrix4x4& mat)
{
	SetUV(index, (mat * MT_Vector4(m_uvs[index][0], m_uvs[index][1], 0.0, 1.0)).getValue());
}
