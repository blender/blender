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

/** \file boolop/intern/BOP_BBox.h
 *  \ingroup boolopintern
 */


#ifndef __BOP_BBOX_H__
#define __BOP_BBOX_H__

#include "MT_Point3.h"
#include "BOP_MathUtils.h"

#define BOP_MAX(a, b) ((a > b) ? a : b)
#define BOP_MIN(a, b) ((a < b) ? a : b)
#define BOP_ABS(a) ((a < 0) ? -(a) : a)

class BOP_BBox
{
public:
	MT_Scalar m_minX;
	MT_Scalar m_minY;
	MT_Scalar m_minZ;
	MT_Scalar m_maxX;
	MT_Scalar m_maxY;
	MT_Scalar m_maxZ;
	MT_Scalar m_centerX;
	MT_Scalar m_centerY;
	MT_Scalar m_centerZ;
	MT_Scalar m_extentX;
	MT_Scalar m_extentY;
	MT_Scalar m_extentZ;
	
public:
	BOP_BBox();
	BOP_BBox(const MT_Point3& p1,const MT_Point3& p2,const MT_Point3& p3);
	inline void add(const MT_Point3& p)
	{
		m_minX = BOP_MIN(m_minX,p[0]);
		m_minY = BOP_MIN(m_minY,p[1]);
		m_minZ = BOP_MIN(m_minZ,p[2]);
		m_maxX = BOP_MAX(m_maxX,p[0]);
		m_maxY = BOP_MAX(m_maxY,p[1]);
		m_maxZ = BOP_MAX(m_maxZ,p[2]);
	};

	inline const MT_Scalar getCenterX() const {return m_centerX;};
	inline const MT_Scalar getCenterY() const {return m_centerY;};
	inline const MT_Scalar getCenterZ() const {return m_centerZ;};

	inline const MT_Scalar getExtentX() const {return m_extentX;};
	inline const MT_Scalar getExtentY() const {return m_extentY;};
	inline const MT_Scalar getExtentZ() const {return m_extentZ;};
	
	inline void compute() {
		m_extentX = (m_maxX-m_minX)/2.0f;
		m_extentY = (m_maxY-m_minY)/2.0f;
		m_extentZ = (m_maxZ-m_minZ)/2.0f;
		m_centerX = m_minX+m_extentX;
		m_centerY = m_minY+m_extentY;
		m_centerZ = m_minZ+m_extentZ;
	};

	inline const bool intersect(const BOP_BBox& b) const {
	  return (!((BOP_comp(m_maxX,b.m_minX)<0) || (BOP_comp(b.m_maxX,m_minX)<0) ||
		    (BOP_comp(m_maxY,b.m_minY)<0) || (BOP_comp(b.m_maxY,m_minY)<0) ||
		    (BOP_comp(m_maxZ,b.m_minZ)<0) || (BOP_comp(b.m_maxZ,m_minZ)<0)));
	};
	
	
};

#endif
