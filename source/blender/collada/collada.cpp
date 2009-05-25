#include "BKE_scene.h"

extern "C"
{
	int collada_import(Scene *sce, const char *filepath)
	{
		return 1;
	}

	int collada_export(Scene *sce, const char *filepath)
	{
		return 1;
	}
}
