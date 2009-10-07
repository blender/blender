#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#ifdef __cplusplus
extern "C" {
#endif	
	
	#include "DNA_listBase.h"
	#include "DNA_scene_types.h"
	
	#include "BKE_context.h"
	
	extern short freestyle_is_initialized;
	
	extern float freestyle_viewpoint[3];
	extern float freestyle_mv[4][4];
	extern float freestyle_proj[4][4];
	extern int freestyle_viewport[4];

	extern char* freestyle_current_module_path;
	extern SceneRenderLayer* freestyle_current_layer;
	extern ListBase* freestyle_modules;
	extern int* freestyle_flags;
	extern float* freestyle_sphere_radius;
	extern float* freestyle_dkr_epsilon;
	
	// Rendering
	void FRS_initialize(bContext* C);
	void FRS_add_Freestyle( struct Render* re);
	void FRS_exit();
	
	// Panel configuration
	void FRS_select_layer( SceneRenderLayer* srl );
	void FRS_add_module();
	void FRS_delete_module(void *module_index_ptr, void *unused);
	void FRS_move_up_module(void *module_index_ptr, void *unused);
	void FRS_move_down_module(void *module_index_ptr, void *unused);
	void FRS_set_module_path(void *module_index_ptr, void *unused);
	
#ifdef __cplusplus
}
#endif

#endif
