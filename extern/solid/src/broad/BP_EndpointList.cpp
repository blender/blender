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

#include <float.h>
#include <algorithm>

#include "BP_EndpointList.h"
#include "BP_Scene.h"
#include "BP_Proxy.h"
#include "BP_ProxyList.h"

DT_Index BP_EndpointList::stab(const BP_Endpoint& pos, BP_ProxyList& proxies) const 
{
	DT_Index result = std::upper_bound(begin(), end(), pos) - begin();
	
	if (result != 0) 
	{
		DT_Index i = result - 1;
		DT_Count count = (*this)[i].getCount(); 
		while (count) 
		{
			const BP_Endpoint& endpoint = (*this)[i];
			if (endpoint.getType() == BP_Endpoint::MINIMUM &&
				pos < (*this)[endpoint.getEndIndex()]) 
			{
				proxies.add(endpoint.getProxy());
				--count;
			}
			assert(i != 0 || count == 0);
			--i;
		}											
	}
	return result;
}

DT_Scalar BP_EndpointList::nextLambda(DT_Index& index, 
									  DT_Scalar source, 
									  DT_Scalar delta) const
{
	if (delta != 0.0f) 
	{
		if (delta < 0.0f) 
		{
			if (index != 0) 
			{
				return ((*this)[--index].getPos() - source) / delta;
			}
		}
		else 
		{
			if (index != size()) 
			{
				return ((*this)[index++].getPos() - source) / delta;
			}
		}
	}
	return FLT_MAX;
}


void BP_EndpointList::range(const BP_Endpoint& min, 
							const BP_Endpoint& max,
							DT_Index& first, DT_Index& last,
							BP_ProxyList& proxies) const 
{
	first = stab(min, proxies);
	last  = std::upper_bound(begin(), end(), max) - begin();
	
	DT_Index i;
	for (i = first; i != last; ++i) 
	{
		const BP_Endpoint& endpoint = (*this)[i];
		if (endpoint.getType() == BP_Endpoint::MINIMUM) 
		{
			proxies.add(endpoint.getProxy());
		}
	}
}

void BP_EndpointList::addInterval(const BP_Endpoint& min, 
								  const BP_Endpoint& max,
								  BP_ProxyList& proxies) 
{
	assert(invariant());
	DT_Index first, last;
	range(min, max, first, last, proxies);
	insert(begin() + last, max);
	insert(begin() + first, min);
	++last; 
	
	(*this)[first].getCount() = first != 0 ? (*this)[first - 1].getCount() : 0;
	(*this)[last].getCount() = (*this)[last - 1].getCount();
	
	
	DT_Index i;
	for (i = first; i != last; ++i) 
	{
		++(*this)[i].getCount();
		(*this)[i].getIndex() = i;
	} 
	for (; i != size(); ++i) 
	{
		(*this)[i].getIndex() = i;
	} 
	
	assert(invariant());
}

void BP_EndpointList::removeInterval(DT_Index first, DT_Index last,
									 BP_ProxyList& proxies) 
{ 
	assert(invariant());
	
	BP_Endpoint min = (*this)[first];
	BP_Endpoint max = (*this)[last]; 
	
	erase(begin() + last);
	erase(begin() + first);
	--last;
	
	DT_Index i;
	for (i = first; i != last; ++i) 
	{
		--(*this)[i].getCount();
		(*this)[i].getIndex() = i;
	} 
	for (; i != size(); ++i) 
	{
		(*this)[i].getIndex() = i;
	} 
	
	range(min, max, first, last, proxies);
	
	assert(invariant());
}

void BP_EndpointList::move(DT_Index index, DT_Scalar pos, Uint32 type,  
						   BP_Scene& scene, T_Overlap overlap)
{
	assert(invariant());
	
	BP_Endpoint endpoint = (*this)[index];
    DT_Scalar delta = pos - endpoint.getPos();
	
    if (delta != DT_Scalar(0.0)) 
	{
		endpoint.setPos(pos, type);
		if (delta < DT_Scalar(0.0)) 
		{
			while (index != 0 && endpoint < (*this)[index - 1]) 
			{
				(*this)[index] = (*this)[index - 1];
				(*this)[index].getIndex() = index;
				encounters((*this)[index], endpoint, scene, overlap);
				--index;
			}
		}
		else 
		{
			DT_Index last = size() - 1;
			while (index != last && (*this)[index + 1] < endpoint) 
			{
				(*this)[index] = (*this)[index + 1];
				(*this)[index].getIndex() = index;
				encounters(endpoint, (*this)[index], scene, overlap);
				++index;
			}
		}
		(*this)[index] = endpoint;
		(*this)[index].getIndex() = index;
    }

	assert(invariant());
}

void BP_EndpointList::encounters(const BP_Endpoint& a, const BP_Endpoint& b,
								 BP_Scene& scene, T_Overlap overlap)
{
	assert(a.getProxy() != b.getProxy());
	
	if (a.getType() != b.getType()) 
	{
		if (a.getType() == BP_Endpoint::MAXIMUM) 
		{
			if (overlap(*a.getProxy(), *b.getProxy())) 
			{
				scene.callBeginOverlap(a.getProxy()->getObject(), 
									   b.getProxy()->getObject());
			}
			++a.getCount();
			++b.getCount();
		}
		else 
		{
			if (overlap(*a.getProxy(), *b.getProxy())) 
			{
				scene.callEndOverlap(a.getProxy()->getObject(), 
									 b.getProxy()->getObject());
			}
			--a.getCount();
			--b.getCount();
		}
	}
	else 
	{
		if (a.getType() == BP_Endpoint::MAXIMUM) 
		{
			--a.getCount();
			++b.getCount();
		}
		else 
		{
			++a.getCount();
			--b.getCount();
		}
	}
}
