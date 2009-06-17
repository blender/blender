#include "BKE_main.h"
#include "BKE_scene.h"
#include "BKE_context.h"

#include "DocumentExporter.h"
#include "DocumentImporter.h"

extern "C"
{
	int collada_import(bContext *C, const char *filepath)
	{
		DocumentImporter imp;
		imp.import(C, filepath);

		return 1;
	}

	int collada_export(Scene *sce, const char *filepath)
	{

		DocumentExporter exp;
		exp.exportCurrentScene(sce, filepath);

		return 1;
	}
}
