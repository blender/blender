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

/** \file RAS_TexVert.h
 *  \ingroup bgerast
 */

#ifndef __RAS_TEXVERT_H__
#define __RAS_TEXVERT_H__


#include "MT_Point3.h"
#include "MT_Point2.h"
#include "MT_Transform.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

static MT_Point3 g_pt3;
static MT_Point2 g_pt2;

class RAS_TexVert
{
	
	float			m_localxyz[3];	// 3*4 = 12
	float			m_uv1[2];		// 2*4 =  8
	float			m_uv2[2];		// 2*4 =  8
	unsigned int	m_rgba;			//        4
	float			m_tangent[4];   // 4*4 =  16
	float			m_normal[3];	// 3*4 =  12
	short			m_flag;			//        2
	short			m_softBodyIndex;		//2
	unsigned int	m_unit;			//		  4
	unsigned int	m_origindex;		//    4
									//---------
									//       56+6+8+2=72
	// 32 bytes total size, fits nice = 56 = not fit nice.

public:
	enum {
		FLAT = 1,
		SECOND_UV = 2,
		MAX_UNIT = 8
	};

	short getFlag() const;
	unsigned int getUnit() const;
	
	RAS_TexVert()// :m_xyz(0,0,0),m_uv(0,0),m_rgba(0)
	{}
	RAS_TexVert(const MT_Point3& xyz,
				const MT_Point2& uv,
				const MT_Point2& uv2,
				const MT_Vector4& tangent,
				const unsigned int rgba,
				const MT_Vector3& normal,
				const bool flat,
				const unsigned int origindex);
	~RAS_TexVert() {};

	const float* getUV1 () const { 
		return m_uv1;
	};

	const float* getUV2 () const { 
		return m_uv2;
	};

	const float* getXYZ() const { 
		return m_localxyz;
	};
	
	const float* getNormal() const {
		return m_normal;
	}
	
	short int getSoftBodyIndex() const
	{
		return m_softBodyIndex;
	}
	
	void	setSoftBodyIndex(short int sbIndex)
	{
		m_softBodyIndex = sbIndex;
	}

	const float* getTangent() const {
		return m_tangent;
	}

	const unsigned char* getRGBA() const {
		return (unsigned char *) &m_rgba;
	}

	unsigned int getOrigIndex() const {
		return m_origindex;
	}

	void				SetXYZ(const MT_Point3& xyz);
	void				SetXYZ(const float *xyz);
	void				SetUV(const MT_Point2& uv);
	void				SetUV2(const MT_Point2& uv);

	void				SetRGBA(const unsigned int rgba);
	void				SetNormal(const MT_Vector3& normal);
	void				SetTangent(const MT_Vector3& tangent);
	void				SetFlag(const short flag);
	void				SetUnit(const unsigned u);
	
	void				SetRGBA(const MT_Vector4& rgba);
	const MT_Point3&	xyz();

	void				Transform(const class MT_Matrix4x4& mat,
	                              const class MT_Matrix4x4& nmat);

	// compare two vertices, to test if they can be shared, used for
	// splitting up based on uv's, colors, etc
	bool				closeTo(const RAS_TexVert* other);


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_TexVert")
#endif
};

#endif //__RAS_TEXVERT_H__

