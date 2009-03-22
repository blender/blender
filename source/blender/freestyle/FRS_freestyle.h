#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#define FREESTYLE_SUGGESTIVE_CONTOURS_FLAG  1
#define FREESTYLE_RIDGES_AND_VALLEYS_FLAG   2

#ifdef __cplusplus
extern "C" {
#endif	
	
	extern char style_module[255];
	extern int freestyle_flags;
	extern float freestyle_sphere_radius;
	extern float freestyle_dkr_epsilon;
	
	extern float freestyle_fovyradian;
	extern float freestyle_viewpoint[3];
	extern float freestyle_mv[4][4];
	extern float freestyle_proj[4][4];
	extern int freestyle_viewport[4];
	
	void FRS_initialize();
	void FRS_prepare(Render* re);
	void FRS_render_Blender(Render* re);
	void FRS_composite_result(Render* re, SceneRenderLayer* srl);
	void FRS_add_Freestyle(Render* re);
	
#ifdef __cplusplus
}
#endif

#endif
