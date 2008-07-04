#ifndef FRS_FREESTYLE_H
#define FRS_FREESTYLE_H

#ifdef __cplusplus
extern "C" {
#endif	
	
	void FRS_prepare(Render* re);
	void FRS_execute(Render* re, int render_in_layer);

#ifdef __cplusplus
}
#endif

#endif
