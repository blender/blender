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
	
	void FRS_initialize();
	void FRS_prepare(Render* re);
	void FRS_render_GL(Render* re);

#ifdef __cplusplus
}
#endif

#endif
