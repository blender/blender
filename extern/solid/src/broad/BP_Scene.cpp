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

#include "BP_Scene.h"
#include "BP_Proxy.h"

#include <algorithm>

BP_Proxy *BP_Scene::createProxy(void *object, 
								const DT_Vector3 min,
								const DT_Vector3 max)
{
	BP_Proxy *proxy = new BP_Proxy(object, *this);

	proxy->add(min, max, m_proxies);
	
	BP_ProxyList::iterator it;
	for (it = m_proxies.begin(); it != m_proxies.end(); ++it)
	{
		if ((*it).second == 3)
		{
			callBeginOverlap(proxy->getObject(), (*it).first->getObject());
		}
	}

	m_proxies.clear();

	return proxy;
}

void BP_Scene::destroyProxy(BP_Proxy *proxy)
{
	proxy->remove(m_proxies);
	
	BP_ProxyList::iterator it;
	for (it = m_proxies.begin(); it != m_proxies.end(); ++it)
	{
		if ((*it).second == 3)
		{
			callEndOverlap(proxy->getObject(), (*it).first->getObject());
		}
	}
	
	m_proxies.clear();

	delete proxy;
}

void *BP_Scene::rayCast(BP_RayCastCallback objectRayCast,
						void *client_data,
						const DT_Vector3 source, 
						const DT_Vector3 target, 
						DT_Scalar& lambda) const 
{
	void *client_object = 0;
	
	DT_Index index[3];
	index[0] = m_endpointList[0].stab(source[0], m_proxies);
	index[1] = m_endpointList[1].stab(source[1], m_proxies);
	index[2] = m_endpointList[2].stab(source[2], m_proxies);

	BP_ProxyList::iterator it;
	for (it = m_proxies.begin(); it != m_proxies.end(); ++it) 
	{
		if ((*it).second == 3 &&
            (*objectRayCast)(client_data, (*it).first->getObject(), source, target, &lambda))
		{
			client_object = (*it).first->getObject();
		}
	}

	DT_Vector3 delta;
	delta[0] = target[0] - source[0];
	delta[1] = target[1] - source[1];
	delta[2] = target[2] - source[2];
	
	DT_Vector3 lambdas;
	lambdas[0] = m_endpointList[0].nextLambda(index[0], source[0], delta[0]);
	lambdas[1] = m_endpointList[1].nextLambda(index[1], source[1], delta[1]);
	lambdas[2] = m_endpointList[2].nextLambda(index[2], source[2], delta[2]);
	int closest = lambdas[0] < lambdas[1] ? (lambdas[0] < lambdas[2] ? 0 : 2) : (lambdas[1] < lambdas[2] ? 1 : 2);
	
	while (lambdas[closest] < lambda)
	{
		if (delta[closest] < 0.0f)
		{
			const BP_Endpoint& endpoint = m_endpointList[closest][index[closest]];

			if (endpoint.getType() == BP_Endpoint::MAXIMUM) 
			{
				it = m_proxies.add(endpoint.getProxy());
				if ((*it).second == 3 &&
					(*objectRayCast)(client_data, (*it).first->getObject(), source, target, &lambda))
				{
					client_object = (*it).first->getObject();
				}
			}
			else
			{
				m_proxies.remove(endpoint.getProxy());
			}
		}
		else 
		{
			const BP_Endpoint& endpoint = m_endpointList[closest][index[closest] - 1];
			
			if (endpoint.getType() == BP_Endpoint::MINIMUM) 
			{
				it = m_proxies.add(endpoint.getProxy());
				if ((*it).second == 3 &&
					(*objectRayCast)(client_data, (*it).first->getObject(), source, target, &lambda))
				{
					client_object = (*it).first->getObject();
				}
			}
			else
			{
				m_proxies.remove(endpoint.getProxy());
			}
		}

		lambdas[closest] = m_endpointList[closest].nextLambda(index[closest], source[closest], delta[closest]);
		closest = lambdas[0] < lambdas[1] ?	(lambdas[0] < lambdas[2] ? 0 : 2) : (lambdas[1] < lambdas[2] ? 1 : 2);
	}

	m_proxies.clear();

	return client_object;
}


