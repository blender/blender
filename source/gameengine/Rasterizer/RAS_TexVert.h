/**
 * $Id$
 *
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
#ifndef __RAS_TEXVERT
#define __RAS_TEXVERT


#include "MT_Point3.h"
#include "MT_Point2.h"
#include "MT_Transform.h"


static MT_Point3 g_pt3;
static MT_Point2 g_pt2;

#define TV_CALCFACENORMAL 0x0001

class RAS_TexVert
{
	
	float			m_localxyz[3];	// 3*4=12 = 24
	float			m_uv1[2];		// 2*4=8 = 24 + 16 = 40
	unsigned int	m_rgba;			//4 = 40 + 4 = 44
	short			m_normal[3];	//3*2=6 = 50 
	short			m_flag;			//32 bytes total size, fits nice = 52 = not fit nice


public:
	short getFlag() const;
	RAS_TexVert()// :m_xyz(0,0,0),m_uv(0,0),m_rgba(0)
	{}
	RAS_TexVert(const MT_Point3& xyz,
				const MT_Point2& uv,
				const unsigned int rgba,
				const short *normal,
				const short flag);
	~RAS_TexVert() {};

	// leave multiline for debugging
	const float* getUV1 () const;

	//const float* getUV1 () const { 
	//	return m_uv1;
	//};

	const MT_Point3&	xyz();

	void				SetXYZ(const MT_Point3& xyz);
	void				SetUV(const MT_Point2& uv);
	void				SetRGBA(const unsigned int rgba);
	void				SetNormal(const MT_Vector3& normal);
	void				SetFlag(const short flag);
	// leave multiline for debugging
	const short*		getNormal() const;
	//const float* getLocalXYZ() const { 
	//	return m_localxyz;
	//};

	const float*		getLocalXYZ() const;
	const unsigned int&	getRGBA() const;
	// compare two vertices, and return TRUE if both are almost identical (they can be shared)
	bool				closeTo(const RAS_TexVert* other);

	bool				closeTo(const MT_Point3& otherxyz,
								const MT_Point2& otheruv,
								const unsigned int otherrgba,
								short othernormal[3]) const;
};

#endif //__RAS_TEXVERT

