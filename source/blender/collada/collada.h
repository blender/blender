#ifndef BLENDER_COLLADA_H
#define BLENDER_COLLADA_H

struct bContext;
struct Scene;

#ifdef __cplusplus
extern "C" {
#endif
	/*
	 * both return 1 on success, 0 on error
	 */
	int collada_import(bContext *C, const char *filepath);
	int collada_export(Scene *sce, const char *filepath);
#ifdef __cplusplus
}
#endif

#endif
