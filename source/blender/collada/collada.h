#ifndef BLENDER_COLLADA_H
#define BLENDER_COLLADA_H

#include "BKE_scene.h"

#ifdef __cplusplus
extern "C" {
#endif
	/*
	 * both return 1 on success, 0 on error
	 */
	int collada_import(Scene *sce, const char *filepath);
	int collada_export(Scene *sce, const char *filepath);
#ifdef __cplusplus
}
#endif

#endif
