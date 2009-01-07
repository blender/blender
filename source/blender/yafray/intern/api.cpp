#include "export_File.h"
#include "export_Plugin.h"

static yafrayFileRender_t byfile;
static yafrayPluginRender_t byplugin;

yafrayRender_t *YAFBLEND = &byplugin;

extern "C"
{
	void YAF_switchPlugin() { YAFBLEND = &byplugin; }
	void YAF_switchFile() { YAFBLEND = &byfile; }
	int YAF_exportScene(Render* re) { return (int)YAFBLEND->exportScene(re); }
}
