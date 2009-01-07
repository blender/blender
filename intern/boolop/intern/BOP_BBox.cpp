/**
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

#include "BOP_BBox.h"

#include "MT_Scalar.h"

/**
 * Constructs a nwe bounding box.
 */
BOP_BBox::BOP_BBox()
{
	m_minX = MT_INFINITY;
	m_minY = MT_INFINITY;
	m_minZ = MT_INFINITY;
	m_maxX = -MT_INFINITY;
	m_maxY = -MT_INFINITY;
	m_maxZ = -MT_INFINITY;
}

/**
 * Constructs a new bounding box using three points.
 * @param p1 first point
 * @param p2 second point
 * @param p3 third point
 */
BOP_BBox::BOP_BBox(const MT_Point3& p1,const MT_Point3& p2,const MT_Point3& p3)
{
  m_minX = BOP_MIN(BOP_MIN(p1[0],p2[0]),p3[0]);
  m_minY = BOP_MIN(BOP_MIN(p1[1],p2[1]),p3[1]);
  m_minZ = BOP_MIN(BOP_MIN(p1[2],p2[2]),p3[2]);
  m_maxX = BOP_MAX(BOP_MAX(p1[0],p2[0]),p3[0]);
  m_maxY = BOP_MAX(BOP_MAX(p1[1],p2[1]),p3[1]);
  m_maxZ = BOP_MAX(BOP_MAX(p1[2],p2[2]),p3[2]);
}
