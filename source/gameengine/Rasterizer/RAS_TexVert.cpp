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

#include "RAS_TexVert.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RAS_TexVert::RAS_TexVert(const MT_Point3& xyz,
						 const MT_Point2& uv,
						 const unsigned int rgba,
						 const short *normal,
						 const short flag) 
{
	xyz.getValue(m_localxyz);
	uv.getValue(m_uv1);
	SetRGBA(rgba);
	m_normal[0] = normal[0];
	m_normal[1] = normal[1];
	m_normal[2] = normal[2];
	m_flag = flag;
}





const MT_Point3& RAS_TexVert::xyz()
{
	g_pt3.setValue(m_localxyz);
	return g_pt3;
}

void RAS_TexVert::SetRGBA(const MT_Vector4& rgba)
{
	unsigned char *colp = (unsigned char*) &m_rgba;
	colp[0] = rgba[0]*255.0;
	colp[1] = rgba[1]*255.0;
	colp[2] = rgba[2]*255.0;
	colp[3] = rgba[3]*255.0;
}

#ifndef RAS_TexVert_INLINE

void RAS_TexVert::SetXYZ(const MT_Point3& xyz)
{
	xyz.getValue(m_localxyz);
}



void RAS_TexVert::SetUV(const MT_Point2& uv)
{
	uv.getValue(m_uv1);
}



void RAS_TexVert::SetRGBA(const unsigned int rgba)
{ 
	m_rgba = rgba;
}


void RAS_TexVert::SetFlag(const short flag)
{
	m_flag = flag;
}
void RAS_TexVert::SetNormal(const MT_Vector3& normal)
{
	m_normal[0] = short(normal.x()*32767.0);
	m_normal[1] = short(normal.y()*32767.0);
	m_normal[2] = short(normal.z()*32767.0);
}


// leave multiline for debugging
const float* RAS_TexVert::getUV1 () const
{
	return m_uv1;
}


const short* RAS_TexVert::getNormal() const
{
	return m_normal;
}



const float* RAS_TexVert::getLocalXYZ() const
{ 
	return m_localxyz;
}
	


const unsigned int& RAS_TexVert::getRGBA() const
{
	return m_rgba;
}

#endif

// compare two vertices, and return TRUE if both are almost identical (they can be shared)
bool RAS_TexVert::closeTo(const RAS_TexVert* other)
{
	return (m_flag == other->m_flag &&
		m_rgba == other->m_rgba &&
		m_normal[0] == other->m_normal[0] &&
		m_normal[1] == other->m_normal[1] &&
		m_normal[2] == other->m_normal[2] &&
		(MT_Vector2(m_uv1) - MT_Vector2(other->m_uv1)).fuzzyZero() &&
		(MT_Vector3(m_localxyz) - MT_Vector3(other->m_localxyz)).fuzzyZero()) ;
	
}



bool RAS_TexVert::closeTo(const MT_Point3& otherxyz,
			 const MT_Point2& otheruv,
			 const unsigned int otherrgba,
			 short othernormal[3]) const
{
	return (m_rgba == otherrgba &&
		m_normal[0] == othernormal[0] &&
		m_normal[1] == othernormal[1] &&
		m_normal[2] == othernormal[2] &&
		(MT_Vector2(m_uv1) - otheruv).fuzzyZero() &&
		(MT_Vector3(m_localxyz) - otherxyz).fuzzyZero()) ;
}


short RAS_TexVert::getFlag() const
{
	return m_flag;
}

void RAS_TexVert::getOffsets(void* &xyz, void* &uv1, void* &rgba, void* &normal) const
{
	xyz = (void *) m_localxyz;
	uv1 = (void *) m_uv1;
	rgba = (void *) m_rgba;
	normal = (void *) m_normal;
}
