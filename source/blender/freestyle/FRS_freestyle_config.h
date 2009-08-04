#ifndef FRS_FREESTYLE_CONFIG_H
#define FRS_FREESTYLE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif	
	
	#include "DNA_scene_types.h"

	void FRS_add_freestyle_config( SceneRenderLayer* srl );
	void FRS_free_freestyle_config( SceneRenderLayer* srl );

#ifdef __cplusplus
}
#endif

#endif