#ifndef SOLID_BROADPHASE_H
#define SOLID_BROADPHASE_H

#include "solid_types.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
    
DT_DECLARE_HANDLE(BP_SceneHandle);
DT_DECLARE_HANDLE(BP_ProxyHandle);

typedef void (*BP_Callback)(void *client_data,
                            void *object1,
                            void *object2);

extern BP_SceneHandle BP_CreateScene(void *client_data,
									 BP_Callback beginOverlap,
									 BP_Callback endOverlap);
 
extern void           BP_DeleteScene(BP_SceneHandle scene);
	
extern BP_ProxyHandle BP_CreateProxy(BP_SceneHandle scene, void *object,
									 const DT_Vector3 lower, 
									 const DT_Vector3 upper);

extern void           BP_DeleteProxy(BP_SceneHandle scene, 
									 BP_ProxyHandle proxy);

extern void BP_SetBBox(BP_ProxyHandle proxy, 
					   const DT_Vector3 lower, 
					   const DT_Vector3 upper);

#ifdef __cplusplus
}
#endif

#endif

