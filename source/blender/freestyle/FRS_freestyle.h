#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#define FREESTYLE_SUGGESTIVE_CONTOURS_FLAG  1
#define FREESTYLE_RIDGES_AND_VALLEYS_FLAG   2

#ifdef __cplusplus
extern "C" {
#endif	
	
	typedef struct StyleModuleConf {
		struct StyleModuleConf *next, *prev;
		
		char module_path[255];
		short is_displayed;
	} StyleModuleConf;
	
	
	extern short freestyle_is_initialized;
	
	extern float freestyle_viewpoint[3];
	extern float freestyle_mv[4][4];
	extern float freestyle_proj[4][4];
	extern int freestyle_viewport[4];
	
	extern short freestyle_current_layer_number;
	extern char* freestyle_current_module_path;
	extern SceneRenderLayer* freestyle_current_layer;
	extern ListBase* freestyle_modules;
	extern int* freestyle_flags;
	extern float* freestyle_sphere_radius;
	extern float* freestyle_dkr_epsilon;
	
	// Rendering
	void FRS_initialize();
	void FRS_add_Freestyle(Render* re);
	void FRS_exit();
	
	// Panel configuration
	void FRS_select_layer( SceneRenderLayer* srl );
	void FRS_delete_layer( SceneRenderLayer* srl, short isDestructor );
	void FRS_add_module();
	void FRS_delete_module(void *module_index_ptr, void *unused);
	void FRS_move_up_module(void *module_index_ptr, void *unused);
	void FRS_move_down_module(void *module_index_ptr, void *unused);
	void FRS_set_module_path(void *module_index_ptr, void *unused);
	
#ifdef __cplusplus
}
#endif

#endif
