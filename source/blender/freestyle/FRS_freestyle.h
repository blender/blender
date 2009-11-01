#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#ifdef __cplusplus
extern "C" {
#endif	
	
	#include "DNA_listBase.h"
	#include "DNA_scene_types.h"
	
	#include "BKE_context.h"
	
	extern float freestyle_viewpoint[3];
	extern float freestyle_mv[4][4];
	extern float freestyle_proj[4][4];
	extern int freestyle_viewport[4];

	// Rendering
	void FRS_initialize(bContext* C);
	void FRS_add_Freestyle( struct Render* re);
	void FRS_exit();
	
	// Panel configuration
	void FRS_add_module(FreestyleConfig *config);
	void FRS_delete_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	void FRS_move_up_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	void FRS_move_down_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	
#ifdef __cplusplus
}
#endif

#endif
