#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#ifdef __cplusplus
extern "C" {
#endif	
	
	#include "DNA_listBase.h"
	#include "DNA_scene_types.h"
	
	#include "BKE_context.h"
	
	extern Scene *freestyle_scene;
	extern float freestyle_viewpoint[3];
	extern float freestyle_mv[4][4];
	extern float freestyle_proj[4][4];
	extern int freestyle_viewport[4];

	// Rendering
	void FRS_initialize();
	void FRS_set_context(bContext* C);
	int FRS_is_freestyle_enabled(struct SceneRenderLayer* srl);
	void FRS_init_stroke_rendering(struct Render* re);
	struct Render* FRS_do_stroke_rendering(struct Render* re, struct SceneRenderLayer* srl);
	void FRS_composite_result(struct Render* re, struct SceneRenderLayer* srl, struct Render* freestyle_render);
	void FRS_exit();
	
	// Panel configuration
	void FRS_add_module(FreestyleConfig *config);
	void FRS_delete_module(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	void FRS_move_module_up(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	void FRS_move_module_down(FreestyleConfig *config, FreestyleModuleConfig *module_conf);
	
	void FRS_add_lineset(FreestyleConfig *config);
	void FRS_delete_active_lineset(FreestyleConfig *config);
	void FRS_move_active_lineset_up(FreestyleConfig *config);
	void FRS_move_active_lineset_down(FreestyleConfig *config);

	FreestyleLineSet *FRS_get_active_lineset(FreestyleConfig *config);
	short FRS_get_active_lineset_index(FreestyleConfig *config);
	void FRS_set_active_lineset_index(FreestyleConfig *config, short index);

#ifdef __cplusplus
}
#endif

#endif
