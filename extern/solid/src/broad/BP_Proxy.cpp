/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#include <new>

#include "BP_Proxy.h"
#include "BP_Scene.h"

BP_Proxy::BP_Proxy(void *object, 
				   BP_Scene& scene) 
  :	m_object(object),
	m_scene(scene)
{
	int i;
	for (i = 0; i < 3; ++i) 
	{
		new (&m_interval[i]) BP_Interval(this);
	}
}

void BP_Proxy::add(const DT_Vector3 min,
				   const DT_Vector3 max,
				   BP_ProxyList& proxies) 
{
	int i;
	for (i = 0; i < 3; ++i) 
	{
		m_scene.getList(i).addInterval(
			BP_Endpoint(min[i], BP_Endpoint::MINIMUM, &m_interval[i].m_min), 
			BP_Endpoint(max[i], BP_Endpoint::MAXIMUM, &m_interval[i].m_max), 
			proxies);
	}
}

void BP_Proxy::remove(BP_ProxyList& proxies) 
{
	int i;
	for (i = 0; i < 3; ++i) 
	{
		m_scene.getList(i).removeInterval(
			m_interval[i].m_min.m_index,
			m_interval[i].m_max.m_index,
			proxies);
	}
}

DT_Scalar BP_Proxy::getMin(int i) const 
{ 
	return m_scene.getList(i)[m_interval[i].m_min.m_index].getPos(); 
}

DT_Scalar BP_Proxy::getMax(int i) const 
{ 
	return m_scene.getList(i)[m_interval[i].m_max.m_index].getPos(); 
}

bool overlapXY(const BP_Proxy& a, const BP_Proxy& b)
{
	return a.getMin(0) <= b.getMax(0) && b.getMin(0) <= a.getMax(0) && 
		   a.getMin(1) <= b.getMax(1) && b.getMin(1) <= a.getMax(1);
}

bool overlapXZ(const BP_Proxy& a, const BP_Proxy& b)
{
	return a.getMin(0) <= b.getMax(0) && b.getMin(0) <= a.getMax(0) && 
		   a.getMin(2) <= b.getMax(2) && b.getMin(2) <= a.getMax(2); 
}

bool overlapYZ(const BP_Proxy& a, const BP_Proxy& b)
{
	return a.getMin(1) <= b.getMax(1) && b.getMin(1) <= a.getMax(1) && 
		   a.getMin(2) <= b.getMax(2) && b.getMin(2) <= a.getMax(2); 
}

void BP_Proxy::setBBox(const DT_Vector3 min, const DT_Vector3 max)
{	
	static T_Overlap overlap[3] = { overlapYZ, overlapXZ, overlapXY };

	int i;
	for (i = 0; i < 3; ++i) 
	{
		if (min[i] > getMax(i)) 
		{
			m_scene.getList(i).move(m_interval[i].m_max.m_index, max[i], 
									BP_Endpoint::MAXIMUM, m_scene, overlap[i]);
			m_scene.getList(i).move(m_interval[i].m_min.m_index, min[i], 
									BP_Endpoint::MINIMUM, m_scene, overlap[i]);
		}
		else 
		{
			m_scene.getList(i).move(m_interval[i].m_min.m_index, min[i], 
									BP_Endpoint::MINIMUM, m_scene, overlap[i]);
			m_scene.getList(i).move(m_interval[i].m_max.m_index, max[i], 
									BP_Endpoint::MAXIMUM, m_scene, overlap[i]);
		}
	}
}



