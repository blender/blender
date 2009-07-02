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

#ifndef SOLID_BROAD_H
#define SOLID_BROAD_H

#include "SOLID_types.h"

#ifdef __cplusplus
extern "C" {
#endif
    
	DT_DECLARE_HANDLE(BP_SceneHandle);
	DT_DECLARE_HANDLE(BP_ProxyHandle);
	
	typedef void (*BP_Callback)(void *client_data,
								void *object1,
								void *object2);

	typedef bool (*BP_RayCastCallback)(void *client_data,
									   void *object,
									   const DT_Vector3 source,
									   const DT_Vector3 target,
									   DT_Scalar *lambda);
	
	extern DECLSPEC BP_SceneHandle BP_CreateScene(void *client_data,
												  BP_Callback beginOverlap,
												  BP_Callback endOverlap);
	
	extern DECLSPEC void           BP_DestroyScene(BP_SceneHandle scene);
	
	extern DECLSPEC BP_ProxyHandle BP_CreateProxy(BP_SceneHandle scene, 
												  void *object,
												  const DT_Vector3 min, 
												  const DT_Vector3 max);
	
	extern DECLSPEC void           BP_DestroyProxy(BP_SceneHandle scene, 
												  BP_ProxyHandle proxy);
	
	extern DECLSPEC void BP_SetBBox(BP_ProxyHandle proxy, 
									const DT_Vector3 min, 
									const DT_Vector3 max);
	
	extern DECLSPEC void *BP_RayCast(BP_SceneHandle scene, 
									 BP_RayCastCallback objectRayCast, 
									 void *client_data,
									 const DT_Vector3 source,
									 const DT_Vector3 target,
									 DT_Scalar *lambda);		
	
#ifdef __cplusplus
}
#endif

#endif
