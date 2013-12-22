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
 * Bounding Box
 */

/** \file gameengine/SceneGraph/SG_BBox.cpp
 *  \ingroup bgesg
 */


#include <math.h>
 
#include "SG_BBox.h"
#include "SG_Node.h"
 
SG_BBox::SG_BBox() :
	m_min(0.0, 0.0, 0.0),
	m_max(0.0, 0.0, 0.0)
{
}

SG_BBox::SG_BBox(const MT_Point3 &min, const MT_Point3 &max) :
	m_min(min),
	m_max(max)
{
}

SG_BBox::SG_BBox(const SG_BBox &other, const MT_Transform &world) :
	m_min(world(other.m_min)),
	m_max(world(other.m_max))
{
	*this += world(MT_Point3(m_min[0], m_min[1], m_max[2]));
	*this += world(MT_Point3(m_min[0], m_max[1], m_min[2]));
	*this += world(MT_Point3(m_min[0], m_max[1], m_max[2]));
	*this += world(MT_Point3(m_max[0], m_min[1], m_min[2]));
	*this += world(MT_Point3(m_max[0], m_min[1], m_max[2]));
	*this += world(MT_Point3(m_max[0], m_max[1], m_min[2]));
}

SG_BBox::SG_BBox(const SG_BBox &other) :
	m_min(other.m_min),
	m_max(other.m_max)
{
}

SG_BBox::~ SG_BBox()
{
}

SG_BBox& SG_BBox::operator +=(const MT_Point3 &point)
{
	if (point[0] < m_min[0])
		m_min[0] = point[0];
	else if (point[0] > m_max[0])
		m_max[0] = point[0];

	if (point[1] < m_min[1])
		m_min[1] = point[1];
	else if (point[1] > m_max[1])
		m_max[1] = point[1];
	
	if (point[2] < m_min[2])
		m_min[2] = point[2];
	else if (point[2] > m_max[2])
		m_max[2] = point[2];
		
	return *this;
}

SG_BBox& SG_BBox::operator += (const SG_BBox &bbox)
{
	*this += bbox.m_min;
	*this += bbox.m_max;
	
	return *this;
}

SG_BBox SG_BBox::operator +(const SG_BBox &bbox2) const
{
	SG_BBox ret = *this;
	ret += bbox2;
	return ret;
}

MT_Scalar SG_BBox::volume() const
{
	MT_Vector3 size = m_max - m_min;
	return size[0]*size[1]*size[2];
}
#if 0
void SG_BBox::translate(const MT_Vector3& dx)
{
	m_min += dx;
	m_max += dx;
}

void SG_BBox::scale(const MT_Vector3& size, const MT_Point3& point)
{
	MT_Vector3 center = (m_max - m_min)/2. + point;
	m_max = (m_max - center)*size;
	m_min = (m_min - center)*size;
}
#endif

SG_BBox SG_BBox::transform(const MT_Transform &world) const
{
	SG_BBox bbox(world(m_min), world(m_max));
	bbox += world(MT_Point3(m_min[0], m_min[1], m_max[2]));
	bbox += world(MT_Point3(m_min[0], m_max[1], m_min[2]));
	bbox += world(MT_Point3(m_min[0], m_max[1], m_max[2]));
	bbox += world(MT_Point3(m_max[0], m_min[1], m_min[2]));
	bbox += world(MT_Point3(m_max[0], m_min[1], m_max[2]));
	bbox += world(MT_Point3(m_max[0], m_max[1], m_min[2]));
	return bbox;
}

bool SG_BBox::inside(const MT_Point3 &point) const
{
	return point[0] >= m_min[0] && point[0] <= m_max[0] &&
	        point[1] >= m_min[1] && point[1] <= m_max[1] &&
	        point[2] >= m_min[2] && point[2] <= m_max[2];
}

bool SG_BBox::inside(const SG_BBox& other) const
{
	return inside(other.m_min) && inside(other.m_max);
}

bool SG_BBox::intersects(const SG_BBox& other) const
{
	return inside(other.m_min) != inside(other.m_max);
}

bool SG_BBox::outside(const SG_BBox& other) const
{
	return !inside(other.m_min) && !inside(other.m_max);
}

SG_BBox::intersect SG_BBox::test(const SG_BBox& other) const
{
	bool point1(inside(other.m_min)), point2(inside(other.m_max));
	
	return point1?(point2?INSIDE:INTERSECT):(point2?INTERSECT:OUTSIDE);
}

void SG_BBox::get(MT_Point3 *box, const MT_Transform &world) const
{
	*box++ = world(m_min);
	*box++ = world(MT_Point3(m_min[0], m_min[1], m_max[2]));
	*box++ = world(MT_Point3(m_min[0], m_max[1], m_min[2]));
	*box++ = world(MT_Point3(m_min[0], m_max[1], m_max[2]));
	*box++ = world(MT_Point3(m_max[0], m_min[1], m_min[2]));
	*box++ = world(MT_Point3(m_max[0], m_min[1], m_max[2]));
	*box++ = world(MT_Point3(m_max[0], m_max[1], m_min[2]));
	*box++ = world(m_max);
}

void SG_BBox::getaa(MT_Point3 *box, const MT_Transform &world) const
{
	const MT_Point3 min(world(m_min)), max(world(m_max));
	*box++ = min;
	*box++ = MT_Point3(min[0], min[1], max[2]);
	*box++ = MT_Point3(min[0], max[1], min[2]);
	*box++ = MT_Point3(min[0], max[1], max[2]);
	*box++ = MT_Point3(max[0], min[1], min[2]);
	*box++ = MT_Point3(max[0], min[1], max[2]);
	*box++ = MT_Point3(max[0], max[1], min[2]);
	*box++ = max;
}

void SG_BBox::getmm(MT_Point3 *box, const MT_Transform &world) const
{
	const MT_Point3 min(world(m_min)), max(world(m_max));
	*box++ = min;
	*box++ = max;
}

void SG_BBox::split(SG_BBox &left, SG_BBox &right) const
{
	MT_Scalar sizex = m_max[0] - m_min[0];
	MT_Scalar sizey = m_max[1] - m_min[1];
	MT_Scalar sizez = m_max[2] - m_min[2];
	if (sizex < sizey)
	{
		if (sizey > sizez)
		{
			left.m_min = m_min;
			left.m_max[0] = m_max[0];
			left.m_max[1] = m_min[1] + sizey/2.0;
			left.m_max[2] = m_max[2];
			
			right.m_min[0] = m_min[0];
			right.m_min[1] = m_min[1] + sizey/2.0;
			right.m_min[2] = m_min[2];
			right.m_max = m_max;
			std::cout << "splity" << std::endl;
		}
		else {
			left.m_min = m_min;
			left.m_max[0] = m_max[0];
			left.m_max[1] = m_max[1];
			left.m_max[2] = m_min[2] + sizez/2.0;
		
			right.m_min[0] = m_min[0];
			right.m_min[1] = m_min[1];
			right.m_min[2] = m_min[2] + sizez/2.0;
			right.m_max = m_max;
			std::cout << "splitz" << std::endl;
		}
	}
	else {
		if (sizex > sizez) {
			left.m_min = m_min;
			left.m_max[0] = m_min[0] + sizex/2.0;
			left.m_max[1] = m_max[1];
			left.m_max[2] = m_max[2];
			
			right.m_min[0] = m_min[0] + sizex/2.0;
			right.m_min[1] = m_min[1];
			right.m_min[2] = m_min[2];
			right.m_max = m_max;
			std::cout << "splitx" << std::endl;
		}
		else {
			left.m_min = m_min;
			left.m_max[0] = m_max[0];
			left.m_max[1] = m_max[1];
			left.m_max[2] = m_min[2] + sizez/2.0;
		
			right.m_min[0] = m_min[0];
			right.m_min[1] = m_min[1];
			right.m_min[2] = m_min[2] + sizez/2.0;
			right.m_max = m_max;
			std::cout << "splitz" << std::endl;
		}
	}
	
	//std::cout << "Left: " << left.m_min << " -> " << left.m_max << " Right: " << right.m_min << " -> " << right.m_max << std::endl;
}
