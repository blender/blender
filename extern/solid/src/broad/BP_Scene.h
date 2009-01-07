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

#ifndef BP_SCENE_H
#define BP_SCENE_H

#include <SOLID_broad.h>

#include "BP_EndpointList.h"
#include "BP_ProxyList.h"

class BP_Proxy;

class BP_Scene {
public:
    BP_Scene(void *client_data,
			 BP_Callback beginOverlap,
			 BP_Callback endOverlap) 
      :	m_client_data(client_data),
		m_beginOverlap(beginOverlap),
		m_endOverlap(endOverlap),
		m_proxies(20)
	{}

    ~BP_Scene() {}

    BP_Proxy *createProxy(void *object, 
						  const DT_Vector3 min,
						  const DT_Vector3 max);

    void destroyProxy(BP_Proxy *proxy);
	
	void *rayCast(BP_RayCastCallback objectRayCast,
				  void *client_data,
				  const DT_Vector3 source, 
				  const DT_Vector3 target, 
				  DT_Scalar& lambda) const;
	
  	void callBeginOverlap(void *object1, void *object2) 
	{
		(*m_beginOverlap)(m_client_data, object1, object2);
	}
	
	void callEndOverlap(void *object1, void *object2) 
	{
		(*m_endOverlap)(m_client_data, object1, object2);
	}
	
	BP_EndpointList& getList(int i) { return m_endpointList[i]; }

private:
	void                    *m_client_data;
	BP_Callback              m_beginOverlap; 
	BP_Callback              m_endOverlap; 
    BP_EndpointList          m_endpointList[3];
	mutable BP_ProxyList     m_proxies;
};

#endif
