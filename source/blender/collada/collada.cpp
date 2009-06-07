#include <stdio.h>
#include "BKE_scene.h"
#include "DocumentExporter.h"

extern "C"
{
	int collada_import(Scene *sce, const char *filepath)
	{
		return 1;
	}

	int collada_export(Scene *sce, const char *filepath)
	{

		DocumentExporter exp;
		exp.exportCurrentScene(sce, filepath);

		return 1;
	}
}
