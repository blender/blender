#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#ifdef __cplusplus
extern "C" {
#endif	
	
	extern char style_module[255];
	
	void FRS_initialize();
	void FRS_prepare(Render* re);
	void FRS_render_GL(Render* re);

#ifdef __cplusplus
}
#endif

#endif
