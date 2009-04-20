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

#include "SOLID_broad.h"

#include "BP_Scene.h"
#include "BP_Proxy.h"

BP_SceneHandle BP_CreateScene(void *client_data,
							  BP_Callback beginOverlap,
							  BP_Callback endOverlap)
{
	return (BP_SceneHandle)new BP_Scene(client_data, 
										beginOverlap, 
										endOverlap);
}

 
void BP_DestroyScene(BP_SceneHandle scene)
{
	delete (BP_Scene *)scene;
}
	

BP_ProxyHandle BP_CreateProxy(BP_SceneHandle scene, void *object,
							  const DT_Vector3 min, const DT_Vector3 max)
{
	return (BP_ProxyHandle)
		((BP_Scene *)scene)->createProxy(object, min, max);
}


void BP_DestroyProxy(BP_SceneHandle scene, BP_ProxyHandle proxy) 
{
	((BP_Scene *)scene)->destroyProxy((BP_Proxy *)proxy);
}



void BP_SetBBox(BP_ProxyHandle proxy, const DT_Vector3 min, const DT_Vector3 max)	
{
	((BP_Proxy *)proxy)->setBBox(min, max);
}

void *BP_RayCast(BP_SceneHandle scene, 
				 BP_RayCastCallback objectRayCast,
				 void *client_data,
				 const DT_Vector3 source,
				 const DT_Vector3 target,
				 DT_Scalar *lambda) 
{
	return ((BP_Scene *)scene)->rayCast(objectRayCast,
										client_data,
										source,	target,
										*lambda);
}

